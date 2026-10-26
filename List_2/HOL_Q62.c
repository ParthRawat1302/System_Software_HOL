#include <stdio.h>
#include <signal.h>
#include <unistd.h>

int main() {
    struct sigaction sa_ignore, sa_default, old_sa;

    sa_ignore.sa_handler = SIG_IGN;
    sigemptyset(&sa_ignore.sa_mask);
    sa_ignore.sa_flags = 0;

    printf("Ignoring SIGINT for 10 seconds. Try Ctrl+C - nothing will happen.\n");
    sigaction(SIGINT, &sa_ignore, &old_sa);
    sleep(10);

    sa_default.sa_handler = SIG_DFL;
    sigemptyset(&sa_default.sa_mask);
    sa_default.sa_flags = 0;

    printf("Resetting SIGINT to default action.\n");
    sigaction(SIGINT, &sa_default, NULL);

    printf("Now try Ctrl+C within the next 10 seconds - it will terminate the program.\n");
    sleep(10);

    printf("No SIGINT received, exiting normally.\n");
    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q62.c -o HOL_Q62
// ./HOL_Q62
// First 10 s: press Ctrl+C -> ignored
// Next 10 s:  press Ctrl+C -> program terminates (default action restored)
