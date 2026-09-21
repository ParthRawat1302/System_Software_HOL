// Record locking (write lock) - locks and modifies one of three fixed-size records
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

struct record {
    int id;
    char name[20];
};

int main() {
    int fd = open("records.dat", O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
        perror("open");
        return 1;
    }

    struct record recs[3] = {
        {1, "Alice"},
        {2, "Bob"},
        {3, "Charlie"}
    };

    // Initialize the file with three records the first time it is run
    if (lseek(fd, 0, SEEK_END) == 0) {
        write(fd, recs, sizeof(recs));
    }

    int index = 1; // record to modify (0-based)

    struct flock lock;
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = index * sizeof(struct record);
    lock.l_len = sizeof(struct record);

    printf("Locking record %d for write...\n", index);
    if (fcntl(fd, F_SETLKW, &lock) == -1) {
        perror("write lock failed");
        close(fd);
        return 1;
    }

    struct record r;
    lseek(fd, index * sizeof(struct record), SEEK_SET);
    read(fd, &r, sizeof(r));
    strcpy(r.name, "Modified");

    lseek(fd, index * sizeof(struct record), SEEK_SET);
    write(fd, &r, sizeof(r));

    printf("Record %d updated: id=%d name=%s\n", index, r.id, r.name);

    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &lock);
    close(fd);
    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q18_a.c -o record_write
// ./record_write
// od -c records.dat        -> shows Alice, "Modified", Charlie records
