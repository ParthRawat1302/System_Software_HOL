// Interval timer - ITIMER_VIRTUAL: decrements only while the process executes in
// user mode, delivers SIGVTALRM. Needs actual CPU-bound work to make progress.
// First expiry after 10 s 10 us of user CPU time, then every 10 s 10 us.
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>

volatile sig_atomic_t fired = 0; // number of expiries so far

void handler(int sig) {
    fired++;
    printf("SIGVTALRM received - ITIMER_VIRTUAL expired! (%d)\n", fired);
}

int main() {
    struct itimerval timer;

    signal(SIGVTALRM, handler);

    timer.it_value.tv_sec = 10;     // first expiry
    timer.it_value.tv_usec = 10;
    timer.it_interval.tv_sec = 10;  // reload value: fire again every 10 s 10 us
    timer.it_interval.tv_usec = 10;

    if (setitimer(ITIMER_VIRTUAL, &timer, NULL) == -1) {
        perror("setitimer");
        return 1;
    }

    printf("ITIMER_VIRTUAL set for 10 s 10 us (repeating) of user CPU time. Burning CPU...\n");
    while (fired < 3) {
        // busy loop so the process actually consumes user-mode CPU time
    }

    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q53_b.c -o HOL_Q53_b
// time ./HOL_Q53_b           -> SIGVTALRM every ~10 s of USER CPU time; exits after 3
//                               ("user" in the time output is about 30 s)
