// Interval timer - ITIMER_REAL: decrements in real (wall-clock) time, delivers SIGALRM.
// First expiry after 10 s 10 us, then it reloads and fires every 10 s 10 us.
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>

volatile sig_atomic_t count = 0; // number of expiries so far

void handler(int sig) {
    count++;
    printf("SIGALRM received - ITIMER_REAL expired! (%d)\n", count);
}

int main() {
    struct itimerval timer;

    signal(SIGALRM, handler);

    timer.it_value.tv_sec = 10;     // first expiry
    timer.it_value.tv_usec = 10;
    timer.it_interval.tv_sec = 10;  // reload value: fire again every 10 s 10 us
    timer.it_interval.tv_usec = 10;

    if (setitimer(ITIMER_REAL, &timer, NULL) == -1) {
        perror("setitimer");
        return 1;
    }

    printf("ITIMER_REAL set for 10 s 10 us (repeating). Waiting for 3 expiries...\n");
    while (count < 3) {
        pause(); // sleeps without using CPU; real time still passes
    }

    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q53_a.c -o HOL_Q53_a
// time ./HOL_Q53_a           -> SIGALRM every ~10 s of wall-clock time; exits after 3
