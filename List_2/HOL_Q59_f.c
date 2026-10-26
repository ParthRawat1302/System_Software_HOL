// Catch SIGVTALRM using setitimer() with ITIMER_VIRTUAL
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>

volatile sig_atomic_t fired = 0;

void handler(int sig) {
    fired = 1;
    printf("Caught SIGVTALRM (signal %d) via setitimer(ITIMER_VIRTUAL)\n", sig);
}

int main() {
    struct itimerval timer = {{0, 0}, {10, 0}};

    signal(SIGVTALRM, handler);
    setitimer(ITIMER_VIRTUAL, &timer, NULL);

    printf("ITIMER_VIRTUAL set for 10 seconds of user CPU time. Burning CPU...\n");
    while (!fired) {
        // busy loop to consume user-mode CPU time
    }

    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q59_f.c -o HOL_Q59_f
// time ./HOL_Q59_f           -> after ~10 s of user CPU time: Caught SIGVTALRM (signal 26)
