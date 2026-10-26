// Catch SIGINT (Ctrl+C) using signal()
#include <stdio.h>
#include <signal.h>
#include <unistd.h>

void handler(int sig) {
    printf("\nCaught SIGINT (signal %d). Press Ctrl+C again won't kill me!\n", sig);
}

int main() {
    signal(SIGINT, handler);

    printf("Running... press Ctrl+C (pid=%d)\n", getpid());
    while (1) {
        sleep(1);
    }

    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q59_b.c -o HOL_Q59_b
// ./HOL_Q59_b                -> press Ctrl+C: handler runs, program keeps going
// Stop it with Ctrl+\ (SIGQUIT) or from another terminal: kill -9 <pid>
