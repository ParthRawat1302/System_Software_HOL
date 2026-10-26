// SIGSTOP cannot be caught, blocked, or ignored - this program will simply be
// stopped by the kernel, and no handler here will ever run for it.
#include <stdio.h>
#include <signal.h>
#include <unistd.h>

void handler(int sig) {
    printf("This will NEVER print - SIGSTOP cannot be caught.\n");
}

int main() {
    // The kernel refuses a handler for SIGSTOP: signal() returns SIG_ERR (EINVAL).
    if (signal(SIGSTOP, handler) == SIG_ERR) {
        perror("signal(SIGSTOP) failed as expected");
    }

    printf("Catcher pid=%d waiting. Run: kill -STOP %d (or ./HOL_Q64_b %d)\n", getpid(), getpid(), getpid());
    while (1) {
        sleep(1);
        printf("Still running (or was stopped and resumed) at pid=%d\n", getpid());
    }

    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q64_a.c -o HOL_Q64_a
// gcc HOL_Q64_b.c -o HOL_Q64_b
// Terminal 1: ./HOL_Q64_a            -> signal(SIGSTOP) fails with "Invalid argument"
// Terminal 2: ./HOL_Q64_b <pid>      -> terminal 1 stops printing: it was NOT caught
//             ps -o pid,stat,cmd -p <pid>   -> STAT shows T (stopped)
//             kill -CONT <pid>       -> it resumes printing
//             kill <pid>             -> terminate it
