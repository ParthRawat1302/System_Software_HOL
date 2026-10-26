// Catch SIGFPE using signal(). Deliberately divides by zero to trigger it.
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>

void handler(int sig) {
    printf("Caught SIGFPE (signal %d)\n", sig);
    exit(1);
}

int main() {
    signal(SIGFPE, handler);

    volatile int a = 10, b = 0; // volatile: force a real division at run time
    int c = a / b; // triggers SIGFPE
    printf("c=%d\n", c);

    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q59_c.c -o HOL_Q59_c
// ./HOL_Q59_c                -> Caught SIGFPE (signal 8)
