// Two-way FIFO communication - process B side. Run HOL_Q38_a.c first (or right after).
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>

#define FIFO_A2B "fifo_a_to_b"
#define FIFO_B2A "fifo_b_to_a"

int main() {
    mkfifo(FIFO_A2B, 0666);
    mkfifo(FIFO_B2A, 0666);

    int fd_in = open(FIFO_A2B, O_RDONLY);
    char buf[256];
    int n = read(fd_in, buf, sizeof(buf));
    printf("B received: %s\n", buf);
    close(fd_in);

    int fd_out = open(FIFO_B2A, O_WRONLY);
    char *msg = "Hello from process B!";
    write(fd_out, msg, strlen(msg) + 1);
    printf("B sent: %s\n", msg);
    close(fd_out);

    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q38_a.c -o HOL_Q38_a
// gcc HOL_Q38_b.c -o HOL_Q38_b
// Terminal 1: ./HOL_Q38_a
// Terminal 2: ./HOL_Q38_b
// Expected: A prints "A received: Hello from process B!" and
//           B prints "B received: Hello from process A!"
