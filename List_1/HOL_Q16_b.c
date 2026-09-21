// Mandatory locking (read lock)
// Run HOL_Q16_a.c first in another terminal while this program is sleeping to
// observe the read lock blocking (or being blocked by) the write lock.
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main() {
    int fd = open("mandatory.txt", O_RDWR | O_CREAT, 0666);
    if (fd == -1) {
        perror("open");
        return 1;
    }

    struct flock lock;
    lock.l_type = F_RDLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;

    if (fcntl(fd, F_SETLK, &lock) == -1) {
        perror("read lock failed");
        close(fd);
        return 1;
    }

    printf("Read lock acquired on mandatory.txt. Sleeping 20 seconds...\n");
    sleep(20);

    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &lock);
    close(fd);
    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q16_a.c -o mand_write
// gcc HOL_Q16_b.c -o mand_read
// ./mand_read &            -> acquires the read lock, sleeps 20s
// ./mand_write              -> blocked/fails while the read lock is held (mandatory mode)
// ps -ef | grep mand_read
