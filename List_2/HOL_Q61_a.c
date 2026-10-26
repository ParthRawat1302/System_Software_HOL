// Catch SIGSEGV using sigaction()
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>

void handler(int sig) {
    printf("Caught SIGSEGV (signal %d) via sigaction\n", sig);
    exit(1);
}

int main() {
    struct sigaction sa;
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGSEGV, &sa, NULL);

    int *volatile p = NULL; // volatile: keep the faulting store even with -O2
    *p = 10;

    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q61_a.c -o HOL_Q61_a
// ./HOL_Q61_a                -> Caught SIGSEGV (signal 11) via sigaction
