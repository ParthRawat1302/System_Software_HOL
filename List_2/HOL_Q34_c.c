// ls -l | wc, implemented using fcntl(F_DUPFD)
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

int main() {
    int fd[2];
    pipe(fd);

    pid_t pid1 = fork();
    if (pid1 == 0) {
        close(fd[0]);
        close(1);
        fcntl(fd[1], F_DUPFD, 0); // lowest free fd is 1 (stdout)
        close(fd[1]);

        execlp("ls", "ls", "-l", NULL);
        perror("execlp ls");
        _exit(1);
    }

    pid_t pid2 = fork();
    if (pid2 == 0) {
        close(fd[1]);
        close(0);
        fcntl(fd[0], F_DUPFD, 0); // lowest free fd is 0 (stdin)
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
// gcc HOL_Q34_c.c -o HOL_Q34_c
// ./HOL_Q34_c
// ls -l | wc          -> compare: both print the same three numbers
//                        (lines, words, characters). This version uses fcntl.
