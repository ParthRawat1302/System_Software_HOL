// Two-way communication needs two pipes: one for each direction.
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

int main() {
    int p2c[2], c2p[2]; // parent->child, child->parent
    char buf[100];

    pipe(p2c);
    pipe(c2p);

    pid_t pid = fork();

    if (pid == -1) {
        perror("fork");
        return 1;
    } else if (pid == 0) {
        close(p2c[1]);
        close(c2p[0]);

        int n = read(p2c[0], buf, sizeof(buf) - 1);
        buf[n] = '\0';
        printf("Child received: %s\n", buf);

        char *reply = "Hello parent, from your child!";
        write(c2p[1], reply, strlen(reply) + 1);

        close(p2c[0]);
        close(c2p[1]);
    } else {
        close(p2c[0]);
        close(c2p[1]);

        char *msg = "Hello child, from your parent!";
        write(p2c[1], msg, strlen(msg) + 1);

        int n = read(c2p[0], buf, sizeof(buf) - 1);
        buf[n] = '\0';
        printf("Parent received: %s\n", buf);

        close(p2c[1]);
        close(c2p[0]);
        wait(NULL);
    }

    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q33.c -o HOL_Q33
// ./HOL_Q33
// Expected: the child prints the parent's message and the parent prints the
// child's reply (two pipes, one per direction).
