// Record locking (read lock) - locks and reads one of three fixed-size records.
// Run HOL_Q18_a.c first so records.dat is populated.
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

struct record {
    int id;
    char name[20];
};

int main() {
    int fd = open("records.dat", O_RDWR);
    if (fd == -1) {
        perror("open (run HOL_Q18_a.c first to create records.dat)");
        return 1;
    }

    int index = 2; // record to read (0-based)

    struct flock lock;
    lock.l_type = F_RDLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = index * sizeof(struct record);
    lock.l_len = sizeof(struct record);

    printf("Locking record %d for read...\n", index);
    if (fcntl(fd, F_SETLKW, &lock) == -1) {
        perror("read lock failed");
        close(fd);
        return 1;
    }

    struct record r;
    lseek(fd, index * sizeof(struct record), SEEK_SET);
    read(fd, &r, sizeof(r));

    printf("Record %d read: id=%d name=%s\n", index, r.id, r.name);

    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &lock);
    close(fd);
    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q18_a.c -o record_write
// gcc HOL_Q18_b.c -o record_read
// ./record_write          -> creates records.dat (run this first)
// ./record_read           -> reads record index 2 (Charlie) under a read lock
