// Sends SIGSTOP to the pid of HOL_Q64_a.c using the kill() system call.
// Usage: ./HOL_Q64_b <pid-of-HOL_Q64_a>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <pid>\n", argv[0]);
        return 1;
    }

    pid_t pid = atoi(argv[1]);

    if (kill(pid, SIGSTOP) == -1) {
        perror("kill");
        return 1;
    }

    printf("Sent SIGSTOP to pid %d. The process should now be stopped (see 'ps' state 'T').\n", pid);
    printf("Resume it with: kill -CONT %d\n", pid);

    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q64_a.c -o HOL_Q64_a
// gcc HOL_Q64_b.c -o HOL_Q64_b
// Terminal 1: ./HOL_Q64_a            (note the pid it prints)
// Terminal 2: ./HOL_Q64_b <pid>
//             ps -o pid,stat,cmd -p <pid>   -> STAT shows T (stopped)
//             kill -CONT <pid> ; kill <pid>
