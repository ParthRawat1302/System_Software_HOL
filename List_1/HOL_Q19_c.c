// Q19(c): Put a process into the STOPPED (T) state using SIGSTOP.
#include <stdio.h>
#include <unistd.h>
#include <signal.h>

int main() {
    printf("PID %d is about to stop itself with SIGSTOP.\n", getpid());
    raise(SIGSTOP); // process suspends here until it receives SIGCONT

    printf("Resumed after SIGCONT.\n");
    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q19_c.c -o stopped_state
// ./stopped_state &
// ps -o pid,stat,cmd -p $!      -> STAT column shows 'T' (stopped)
// kill -CONT $!                 -> resumes it; prints "Resumed after SIGCONT."
