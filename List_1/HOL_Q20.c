#include <stdio.h>
#include <unistd.h>

int main() {
    pid_t pid = fork();

    if (pid == -1) {
        perror("fork");
        return 1;
    } else if (pid == 0) {
        printf("Child process: pid=%d, parent pid=%d\n", getpid(), getppid());
    } else {
        printf("Parent process: pid=%d, child pid=%d\n", getpid(), pid);
    }

    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q20.c -o fork_pid
// ./fork_pid
// echo "Shell PID: $$"    -> confirms the parent's parent is your shell
