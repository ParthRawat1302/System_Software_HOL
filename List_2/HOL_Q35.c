// Equivalent of: ls -l | grep ^d | wc -l   using two pipes and dup2 only.
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    int p1[2], p2[2];
    pipe(p1);
    pipe(p2);

    pid_t pid1 = fork();
    if (pid1 == 0) {
        // ls -l
        close(p1[0]);
        dup2(p1[1], 1);
        close(p1[1]);
        close(p2[0]); close(p2[1]);

        execlp("ls", "ls", "-l", NULL);
        perror("execlp ls");
        _exit(1);
    }

    pid_t pid2 = fork();
    if (pid2 == 0) {
        // grep ^d
        close(p1[1]);
        dup2(p1[0], 0);
        close(p1[0]);

        close(p2[0]);
        dup2(p2[1], 1);
        close(p2[1]);

        execlp("grep", "grep", "^d", NULL);
        perror("execlp grep");
        _exit(1);
    }

    pid_t pid3 = fork();
    if (pid3 == 0) {
        // wc -l
        close(p2[1]);
        dup2(p2[0], 0);
        close(p2[0]);
        close(p1[0]); close(p1[1]);

        execlp("wc", "wc", "-l", NULL);
        perror("execlp wc");
        _exit(1);
    }

    close(p1[0]); close(p1[1]);
    close(p2[0]); close(p2[1]);

    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);
    waitpid(pid3, NULL, 0);

    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q35.c -o HOL_Q35
// mkdir -p d1 d2 d3          (optional: create a few directories to count)
// ./HOL_Q35
// ls -l | grep ^d | wc -l    -> compare: same number of directories
// ls -l | grep ^d | wc       -> the first number is the same count
