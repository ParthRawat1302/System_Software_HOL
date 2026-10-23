#include "storage.h"
#include "lock_table.h"
#include "journal.h"
#include "server.h"
#include "auth_guard.h"
#include "session_guard.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>

static void shutdown_handler(int signum) {
    (void)signum;
    journal_close();
    _exit(0);
}

int main(void) {
    /* WSL commonly defaults to UTC even when the Windows host uses IST.
     * Keep transaction timestamps in India time unless the operator sets TZ. */
    if (getenv("TZ") == NULL && setenv("TZ", "Asia/Kolkata", 0) != 0) {
        perror("setenv TZ");
        return 1;
    }
    tzset();

    /* Ignore SIGPIPE process-wide. Without this, a single client that
     * sends a request and then disconnects before reading the reply kills
     * the ENTIRE server: write() to that dead socket raises SIGPIPE, whose
     * default action is to terminate the process -- taking every other
     * logged-in user's session down with it. The
     * window is easy to hit in practice (a client that gives up while
     * queued in the listen backlog, a browser tab closed mid-request), and
     * was reproduced deliberately -- see docs/VIVA_GUIDE.md.
     *
     * With SIG_IGN, write() instead returns -1/EPIPE, which write_full()
     * in protocol.c already propagates; that client's thread then simply
     * exits its loop, releasing its session-guard slot and semaphore slot
     * like any other disconnect. */
    signal(SIGPIPE, SIG_IGN);

    if (storage_init() != 0) {
        fprintf(stderr, "fatal: storage_init failed\n");
        return 1;
    }
    lock_table_init();
    auth_guard_init();
    session_guard_init();
    if (journal_init_and_recover() != 0) {
        fprintf(stderr, "fatal: WAL recovery failed; refusing to serve data\n");
        return 1;
    }
    if (journal_start_recovery_worker(60) != 0) {
        fprintf(stderr, "fatal: could not start recovery worker\n");
        journal_close();
        return 1;
    }
    printf("[main] WAL recovery complete; durable ledger enabled\n");

    signal(SIGINT, shutdown_handler);
    signal(SIGTERM, shutdown_handler);

    int rc = server_run();
    journal_close();
    return rc;
}
