// Two-way FIFO communication needs two FIFOs (one per direction).
// Run this first, then HOL_Q38_b.c in another terminal.
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

    int fd_out = open(FIFO_A2B, O_WRONLY);
    char *msg = "Hello from process A!";
    write(fd_out, msg, strlen(msg) + 1);
    printf("A sent: %s\n", msg);
    close(fd_out);

    int fd_in = open(FIFO_B2A, O_RDONLY);
    char buf[256];
    int n = read(fd_in, buf, sizeof(buf));
    printf("A received: %s\n", buf);
    close(fd_in);

    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q38_a.c -o HOL_Q38_a
// gcc HOL_Q38_b.c -o HOL_Q38_b
// Terminal 1: ./HOL_Q38_a      (sends to B, then waits for B's reply)
// Terminal 2: ./HOL_Q38_b      (receives from A, then replies)
// rm fifo_a_to_b fifo_b_to_a   (clean up)
