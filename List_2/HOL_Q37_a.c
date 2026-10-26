// One-way FIFO communication: writer side. Run HOL_Q37_b.c in another terminal first.
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>

#define FIFO_NAME "one_way_fifo"

int main() {
    mkfifo(FIFO_NAME, 0666); // ok if it already exists

    int fd = open(FIFO_NAME, O_WRONLY);
    if (fd == -1) {
        perror("open");
        return 1;
    }

    char *msg = "Hello from the writer process!";
    write(fd, msg, strlen(msg) + 1);
    printf("Sent: %s\n", msg);

    close(fd);
    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q37_a.c -o HOL_Q37_a
// gcc HOL_Q37_b.c -o HOL_Q37_b
// Terminal 1: ./HOL_Q37_b      (reader, blocks until a writer opens the FIFO)
// Terminal 2: ./HOL_Q37_a      (writer)
// ls -l one_way_fifo           -> 'p' file type
// rm one_way_fifo
