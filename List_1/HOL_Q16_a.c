// Mandatory locking (write lock)
// For the kernel to enforce this lock on another process, the file needs
// the set-group-ID bit set with group-execute cleared: chmod g+s,g-x mandatory.txt
// and the filesystem must be mounted with the "mand" option.
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
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0; // 0 = lock whole file

    if (fcntl(fd, F_SETLK, &lock) == -1) {
        perror("write lock failed");
        close(fd);
        return 1;
    }

    printf("Write lock acquired on mandatory.txt. Sleeping 20 seconds...\n");
    sleep(20);

    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &lock);
    close(fd);
    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q16_a.c -o mand_write
// gcc HOL_Q16_b.c -o mand_read
//
// To make the kernel actually ENFORCE the lock (mandatory locking):
// touch mandatory.txt
// chmod g+s,g-x mandatory.txt
// sudo mount -o remount,mand /            (filesystem must support -o mand)
//
// ./mand_write &          -> acquires the write lock, sleeps 20s
// ./mand_read              -> blocked/fails while the write lock is held
// ps -ef | grep mand_write
