// One-way FIFO communication: reader side. Start this before HOL_Q37_a.c.
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#define FIFO_NAME "one_way_fifo"

int main() {
    mkfifo(FIFO_NAME, 0666); // ok if it already exists

    int fd = open(FIFO_NAME, O_RDONLY);
    if (fd == -1) {
        perror("open");
        return 1;
    }

    char buf[256];
    int n = read(fd, buf, sizeof(buf));
    printf("Received: %s\n", buf);

    close(fd);
    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q37_a.c -o HOL_Q37_a
// gcc HOL_Q37_b.c -o HOL_Q37_b
// Terminal 1: ./HOL_Q37_b      (this reader, blocks until the writer opens the FIFO)
// Terminal 2: ./HOL_Q37_a      (writer)
// Expected in terminal 1: Received: Hello from the writer process!
