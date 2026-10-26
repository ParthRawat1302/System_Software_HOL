// Catch SIGPROF using setitimer() with ITIMER_PROF
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>

volatile sig_atomic_t fired = 0;

void handler(int sig) {
    fired = 1;
    printf("Caught SIGPROF (signal %d) via setitimer(ITIMER_PROF)\n", sig);
}

int main() {
    struct itimerval timer = {{0, 0}, {10, 0}};

    signal(SIGPROF, handler);
    setitimer(ITIMER_PROF, &timer, NULL);

    printf("ITIMER_PROF set for 10 seconds of total CPU time. Working...\n");
    while (!fired) {
        getpid();
    }

    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q59_g.c -o HOL_Q59_g
// time ./HOL_Q59_g           -> after ~10 s of CPU time: Caught SIGPROF (signal 27)
