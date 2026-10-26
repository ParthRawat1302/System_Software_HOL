// Catch SIGSEGV using signal(). Deliberately dereferences a NULL pointer to trigger it.
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>

void handler(int sig) {
    printf("Caught SIGSEGV (signal %d)\n", sig);
    exit(1); // memory is corrupt at this point; must not return normally
}

int main() {
    signal(SIGSEGV, handler);

    int *volatile p = NULL; // volatile: keep the faulting store even with -O2
    *p = 10; // triggers SIGSEGV

    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q59_a.c -o HOL_Q59_a
// ./HOL_Q59_a                -> Caught SIGSEGV (signal 11)
