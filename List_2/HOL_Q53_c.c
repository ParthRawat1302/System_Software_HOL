// Interval timer - ITIMER_PROF: decrements while the process runs in user mode
// AND while the kernel runs on its behalf (system calls), delivers SIGPROF.
// First expiry after 10 s 10 us of total CPU time, then every 10 s 10 us.
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>

volatile sig_atomic_t fired = 0; // number of expiries so far

void handler(int sig) {
    fired++;
    printf("SIGPROF received - ITIMER_PROF expired! (%d)\n", fired);
}

int main() {
    struct itimerval timer;

    signal(SIGPROF, handler);

    timer.it_value.tv_sec = 10;     // first expiry
    timer.it_value.tv_usec = 10;
    timer.it_interval.tv_sec = 10;  // reload value: fire again every 10 s 10 us
    timer.it_interval.tv_usec = 10;

    if (setitimer(ITIMER_PROF, &timer, NULL) == -1) {
        perror("setitimer");
        return 1;
    }

    printf("ITIMER_PROF set for 10 s 10 us (repeating) of total CPU time. Working...\n");
    while (fired < 3) {
        getpid(); // mix of user-mode work and a system call to burn both modes
    }

    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q53_c.c -o HOL_Q53_c
// time ./HOL_Q53_c           -> SIGPROF every ~10 s of USER + SYSTEM CPU time; exits after 3
//                               ("user" + "sys" in the time output is about 30 s)
