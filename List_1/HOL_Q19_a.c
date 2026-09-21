// Q19(a): Put a process into the RUNNING (R) state using a CPU-bound busy loop.
#include <stdio.h>
#include <unistd.h>

int main() {
    printf("PID %d is now busy-looping to stay in the RUNNING state.\n", getpid());
    printf("Check its state from another terminal - it loops forever until killed.\n");

    volatile unsigned long i = 0;
    while (1) {
        i++; // pure CPU work - keeps the process runnable/running, no blocking syscalls
    }

    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q19_a.c -o running_state
// ./running_state &
// ps -o pid,stat,cmd -p $!      -> STAT column shows 'R' (running/runnable)
// top -p $!                     -> also shows R and ~100% CPU usage
// kill $!
