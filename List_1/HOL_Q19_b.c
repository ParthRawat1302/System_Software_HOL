// Q19(b): Put a process into the SLEEPING (S) state using sleep().
#include <stdio.h>
#include <unistd.h>

int main() {
    printf("PID %d is going to sleep for 30 seconds (SLEEPING state).\n", getpid());
    sleep(30); // blocked inside a system call - interruptible sleep
    printf("Woke up.\n");
    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q19_b.c -o sleeping_state
// ./sleeping_state &
// ps -o pid,stat,cmd -p $!      -> STAT column shows 'S' (interruptible sleep)
