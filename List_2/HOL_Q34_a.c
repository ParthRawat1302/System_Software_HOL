// ls -l | wc, implemented using dup()
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    int fd[2];
    pipe(fd);

    pid_t pid1 = fork();
    if (pid1 == 0) {
        // Child 1: ls -l, output goes into the pipe
        close(fd[0]);
        int old_stdout = dup(1);   // save stdout
        close(1);
        dup(fd[1]);                // stdout now points to pipe write end (lowest free fd = 1)
        close(fd[1]);

        execlp("ls", "ls", "-l", NULL);
        perror("execlp ls");
        dup2(old_stdout, 1);
        _exit(1);
    }

    pid_t pid2 = fork();
    if (pid2 == 0) {
        // Child 2: wc, input comes from the pipe
        close(fd[1]);
        int old_stdin = dup(0);
        close(0);
        dup(fd[0]);
        close(fd[0]);

        execlp("wc", "wc", NULL);
        perror("execlp wc");
        dup2(old_stdin, 0);
        _exit(1);
    }

    close(fd[0]);
    close(fd[1]);
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);

    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q34_a.c -o HOL_Q34_a
// ./HOL_Q34_a
// ls -l | wc          -> compare: both print the same three numbers
//                        (lines, words, characters). This version uses dup.
