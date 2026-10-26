// Catch SIGINT using sigaction()
#include <stdio.h>
#include <signal.h>
#include <unistd.h>

void handler(int sig) {
    printf("\nCaught SIGINT (signal %d) via sigaction\n", sig);
}

int main() {
    struct sigaction sa;
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGINT, &sa, NULL);

    printf("Running... press Ctrl+C (pid=%d)\n", getpid());
    while (1) {
        sleep(1);
    }

    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q61_b.c -o HOL_Q61_b
// ./HOL_Q61_b                -> press Ctrl+C: handler runs, program keeps going
// Stop it with Ctrl+\ (SIGQUIT) or from another terminal: kill -9 <pid>
