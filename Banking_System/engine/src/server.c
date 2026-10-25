#include "common.h"
#include "server.h"
#include "protocol.h"
#include "session.h"
#include "session_guard.h"
#include "role_dispatch.h"
#include "storage.h"

#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>

typedef struct {
    int fd;
} ClientArg;

static sem_t g_conn_sem;

static int get_client_idle_timeout(long *seconds) {
    const char *configured = getenv("ENGINE_IDLE_TIMEOUT_SECONDS");
    if (!configured || !*configured) {
        *seconds = CLIENT_IDLE_TIMEOUT_SECONDS;
        return 0;
    }

    errno = 0;
    char *end = NULL;
    long value = strtol(configured, &end, 10);
    if (errno == ERANGE || end == configured || *end != '\0' || value <= 0) {
        fprintf(stderr, "invalid ENGINE_IDLE_TIMEOUT_SECONDS: %s\n", configured);
        return -1;
    }
    *seconds = value;
    return 0;
}

static int set_client_idle_timeout(int fd, long seconds) {
    struct timeval timeout = { .tv_sec = seconds, .tv_usec = 0 };
    return setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
}

static void release_session_guard(ClientSession *sess) {
    if (!sess->guard_acquired) return;
    if (sess->role == SESSION_USER) {
        session_guard_release(GUARD_USER, sess->account_id, sess->as_owner2 ? 2 : 1);
    } else if (sess->role == SESSION_STAFF) {
        session_guard_release(GUARD_STAFF,
                              storage_staff_lock_id(sess->staff_role, sess->staff_id), 0);
    } else if (sess->role == SESSION_ADMIN) {
        session_guard_release(GUARD_ADMIN, sess->admin_id, 0);
    }
    sess->guard_acquired = 0;
}


static void *client_thread(void *arg) {
    ClientArg *carg = (ClientArg *)arg;
    int fd = carg->fd;
    free(carg);

    ClientSession sess;
    memset(&sess, 0, sizeof(sess));

    unsigned char opcode;
    char payload[MAX_PAYLOAD];
    int should_exit = 0;

    while (!should_exit && recv_frame(fd, &opcode, payload, sizeof(payload)) == 0) {
        if (opcode == OP_LOGOUT) {
                /* Release the identity-uniqueness slot (if held) *before*
                 * replying, so the caller has a synchronous guarantee:
                 * once "OK|bye" is received, that identity is immediately
                 * free to log in again from anywhere. */
                release_session_guard(&sess);
                send_frame(fd, OP_LOGOUT, "OK|bye");
                should_exit = 1;
        } else if (!auth_dispatch(fd, opcode, payload, &sess) &&
                   !customer_dispatch(fd, opcode, payload, &sess) &&
                   !employee_dispatch(fd, opcode, payload, &sess) &&
                   !manager_dispatch(fd, opcode, payload, &sess) &&
                   !staff_shared_dispatch(fd, opcode, payload, &sess) &&
                   !admin_dispatch(fd, opcode, payload, &sess)) {
            send_frame(fd, opcode, "ERR|unknown opcode");
        }
    }

    /* Covers the case where the client just dropped the connection
     * without sending LOGOUT -- release_session_guard() is a no-op if
     * OP_LOGOUT already released it above. */
    release_session_guard(&sess);
    close(fd);
    sem_post(&g_conn_sem);
    return NULL;
}

int server_run(void) {
    long idle_timeout_seconds;
    if (get_client_idle_timeout(&idle_timeout_seconds) != 0) return -1;

    if (sem_init(&g_conn_sem, 0, MAX_CONNECTIONS) != 0) {
        perror("sem_init");
        return -1;
    }

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) { perror("socket"); return -1; }

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    /* Loopback-only: the gateway and engine are meant to run on the same
     * host, and this is an unauthenticated-transport, unencrypted custom
     * protocol -- there's no reason to expose it beyond localhost. */
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    addr.sin_port = htons(SERVER_PORT);

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); close(listen_fd); return -1;
    }
    if (listen(listen_fd, 32) < 0) {
        perror("listen"); close(listen_fd); return -1;
    }

    printf("[server] listening on 127.0.0.1:%d (max %d concurrent connections)\n", SERVER_PORT, MAX_CONNECTIONS);

    for (;;) {
        sem_wait(&g_conn_sem); /* blocks here once MAX_CONNECTIONS are already being served */

        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) { perror("accept"); sem_post(&g_conn_sem); continue; }

        if (set_client_idle_timeout(client_fd, idle_timeout_seconds) != 0) {
            perror("setsockopt(SO_RCVTIMEO)");
            close(client_fd);
            sem_post(&g_conn_sem);
            continue;
        }

        ClientArg *carg = malloc(sizeof(ClientArg));
        if (!carg) {
            perror("malloc");
            close(client_fd);
            sem_post(&g_conn_sem);
            continue;
        }
        carg->fd = client_fd;

        pthread_t tid;
        if (pthread_create(&tid, NULL, client_thread, carg) != 0) {
            perror("pthread_create");
            close(client_fd);
            free(carg);
            sem_post(&g_conn_sem);
            continue;
        }
        pthread_detach(tid);
    }
}
