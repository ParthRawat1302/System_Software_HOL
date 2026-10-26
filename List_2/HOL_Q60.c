#include <stdio.h>
#include <signal.h>
#include <unistd.h>

int main() {
    printf("Ignoring SIGINT for 10 seconds. Try Ctrl+C - nothing will happen.\n");
    signal(SIGINT, SIG_IGN);
    sleep(10);

    printf("Resetting SIGINT to default action.\n");
    signal(SIGINT, SIG_DFL);

    printf("Now try Ctrl+C within the next 10 seconds - it will terminate the program.\n");
    sleep(10);

    printf("No SIGINT received, exiting normally.\n");
    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q60.c -o HOL_Q60
// ./HOL_Q60
// First 10 s: press Ctrl+C -> ignored (only ^C is echoed)
// Next 10 s:  press Ctrl+C -> program terminates (default action restored)
