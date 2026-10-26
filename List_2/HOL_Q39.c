// Waits up to 10 seconds for data to be written into a FIFO, using select().
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/select.h>

#define FIFO_NAME "select_fifo"

int main() {
    mkfifo(FIFO_NAME, 0666);

    // O_RDWR avoids blocking on open() while waiting for a writer
    int fd = open(FIFO_NAME, O_RDWR);
    if (fd == -1) {
        perror("open");
        return 1;
    }

    fd_set readfds;
    struct timeval tv;

    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);
    tv.tv_sec = 10;
    tv.tv_usec = 0;

    printf("Waiting up to 10 seconds for data on %s...\n", FIFO_NAME);
    printf("(from another terminal: echo hello > %s)\n", FIFO_NAME);

    int ret = select(fd + 1, &readfds, NULL, NULL, &tv);

    if (ret == -1) {
        perror("select");
    } else if (ret == 0) {
        printf("Timeout! No data received within 10 seconds.\n");
    } else {
        char buf[256];
        int n = read(fd, buf, sizeof(buf) - 1);
        buf[n] = '\0';
        printf("Data received: %s\n", buf);
    }

    close(fd);
    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q39.c -o HOL_Q39
// Case 1 (data arrives):
//   Terminal 1: ./HOL_Q39
//   Terminal 2 (within 10 s): echo hello > select_fifo
// Case 2 (timeout): ./HOL_Q39 and do nothing for 10 seconds
// rm select_fifo
