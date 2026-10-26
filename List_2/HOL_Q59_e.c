// Catch SIGALRM using setitimer() with ITIMER_REAL
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>

void handler(int sig) {
    printf("Caught SIGALRM (signal %d) via setitimer(ITIMER_REAL)\n", sig);
}

int main() {
    struct itimerval timer = {{0, 0}, {10, 0}};

    signal(SIGALRM, handler);
    setitimer(ITIMER_REAL, &timer, NULL);

    printf("ITIMER_REAL set for 10 seconds. Waiting...\n");
    pause();

    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q59_e.c -o HOL_Q59_e
// ./HOL_Q59_e                -> after 10 s: Caught SIGALRM (signal 14) via setitimer
