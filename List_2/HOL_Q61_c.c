// Catch SIGFPE using sigaction()
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>

void handler(int sig) {
    printf("Caught SIGFPE (signal %d) via sigaction\n", sig);
    exit(1);
}

int main() {
    struct sigaction sa;
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGFPE, &sa, NULL);

    volatile int a = 10, b = 0; // volatile: force a real division at run time
    int c = a / b;
    printf("c=%d\n", c);

    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q61_c.c -o HOL_Q61_c
// ./HOL_Q61_c                -> Caught SIGFPE (signal 8) via sigaction
