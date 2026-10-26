// Catch SIGALRM using the alarm() system call
#include <stdio.h>
#include <signal.h>
#include <unistd.h>

void handler(int sig) {
    printf("Caught SIGALRM (signal %d) via alarm()\n", sig);
}

int main() {
    signal(SIGALRM, handler);

    alarm(10);
    printf("Alarm set for 10 seconds. Waiting...\n");
    pause();

    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q59_d.c -o HOL_Q59_d
// ./HOL_Q59_d                -> after 10 s: Caught SIGALRM (signal 14) via alarm()
