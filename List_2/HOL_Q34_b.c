// ls -l | wc, implemented using dup2()
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    int fd[2];
    pipe(fd);

    pid_t pid1 = fork();
    if (pid1 == 0) {
        close(fd[0]);
        dup2(fd[1], 1); // stdout -> pipe write end
        close(fd[1]);

        execlp("ls", "ls", "-l", NULL);
        perror("execlp ls");
        _exit(1);
    }

    pid_t pid2 = fork();
    if (pid2 == 0) {
        close(fd[1]);
        dup2(fd[0], 0); // stdin -> pipe read end
        close(fd[0]);

        execlp("wc", "wc", NULL);
        perror("execlp wc");
        _exit(1);
    }

    close(fd[0]);
    close(fd[1]);
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);

    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q34_b.c -o HOL_Q34_b
// ./HOL_Q34_b
// ls -l | wc          -> compare: both print the same three numbers
//                        (lines, words, characters). This version uses dup2.
