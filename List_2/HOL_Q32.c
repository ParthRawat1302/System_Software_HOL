#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

int main() {
    int fd[2];
    char buf[100];

    pipe(fd);

    pid_t pid = fork();

    if (pid == -1) {
        perror("fork");
        return 1;
    } else if (pid == 0) {
        // Child: reads from the parent
        close(fd[1]); // close unused write end
        int n = read(fd[0], buf, sizeof(buf) - 1);
        buf[n] = '\0';
        printf("Child received: %s\n", buf);
        close(fd[0]);
    } else {
        // Parent: writes to the child
        close(fd[0]); // close unused read end
        char *msg = "Hello child, from your parent!";
        write(fd[1], msg, strlen(msg) + 1);
        close(fd[1]);
        wait(NULL); // wait so the child's output appears before the shell prompt
    }

    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q32.c -o HOL_Q32
// ./HOL_Q32
// Expected: Child received: Hello child, from your parent!
