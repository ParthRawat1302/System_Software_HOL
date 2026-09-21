// Online ticket reservation: read, lock, increment and store the next ticket number.
// Run this concurrently from multiple terminals to see the write lock serialize access.
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

int main() {
    int fd = open("ticket.txt", O_RDWR);
    if (fd == -1) {
        perror("open");
        return 1;
    }

    struct flock lock;
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;

    if (fcntl(fd, F_SETLKW, &lock) == -1) { // F_SETLKW blocks until the lock is available
        perror("lock failed");
        close(fd);
        return 1;
    }

    char buf[16] = {0};
    lseek(fd, 0, SEEK_SET);
    int n = read(fd, buf, sizeof(buf) - 1);
    buf[n] = '\0';
    int ticket = atoi(buf);
    ticket++;

    char newbuf[16];
    int len = sprintf(newbuf, "%d", ticket);

    lseek(fd, 0, SEEK_SET);
    ftruncate(fd, 0);
    write(fd, newbuf, len);

    printf("New ticket number: %d\n", ticket);

    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &lock);
    close(fd);
    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q17_b.c -o update_ticket
// ./update_ticket
// cat ticket.txt
//
// Run several at once to show the write lock serializing access safely:
// for i in 1 2 3 4 5; do ./update_ticket & done; wait
// cat ticket.txt    -> incremented exactly 5 times, no lost updates
