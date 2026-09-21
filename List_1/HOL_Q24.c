#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    pid_t pids[3];

    for (int i = 0; i < 3; i++) {
        pids[i] = fork();
        if (pids[i] == -1) {
            perror("fork");
            return 1;
        } else if (pids[i] == 0) {
            printf("Child %d: pid=%d\n", i, getpid());
            sleep(2 + i);
            _exit(0);
        }
    }

    // Parent waits specifically for child 1 (middle child), not just any child
    pid_t target = pids[1];
    waitpid(target, NULL, 0);
    printf("Parent: child 1 (pid=%d) finished.\n", target);

    // Reap the remaining children so they don't linger as zombies
    for (int i = 0; i < 3; i++) {
        if (pids[i] != target) waitpid(pids[i], NULL, 0);
    }

    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q24.c -o waitpid_children
// ./waitpid_children
// -> "Parent: child 1 finished" prints after ~3s, before child 2 (5s) finishes,
//    proving waitpid() targeted that specific pid rather than any child
