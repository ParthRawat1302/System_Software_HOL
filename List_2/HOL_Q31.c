// cd "/mnt/c/Users/Parth/Desktop/System Design/Banking_System/engine"

#include <stdio.h>
#include <unistd.h>
#include <string.h>

int main() {
    int fd[2];
    char buf[100];

    if (pipe(fd) == -1) {
        perror("pipe");
        return 1;
    }

    char *msg = "Hello through the pipe!";
    write(fd[1], msg, strlen(msg) + 1);

    int n = read(fd[0], buf, sizeof(buf));
    printf("Read %d bytes from pipe: %s\n", n, buf);

    close(fd[0]);
    close(fd[1]);
    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q31.c -o HOL_Q31
// ./HOL_Q31
// Expected: Read 24 bytes from pipe: Hello through the pipe!
