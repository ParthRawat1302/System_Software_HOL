#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#define FIFO_NAME "myfifo"
#define BLOCK_SIZE 32

int main(void) {
    int fd = open(FIFO_NAME, O_RDONLY);

    if (fd == -1) {
        perror("open");
        return 1;
    }

    for (int i = 0; i < 3; i++) {
        char block[BLOCK_SIZE];
        ssize_t bytes_read = 0;

        while (bytes_read < BLOCK_SIZE) {
            ssize_t result = read(fd, block + bytes_read,
                                  BLOCK_SIZE - bytes_read);
            if (result <= 0) {
                perror("read");
                close(fd);
                return 1;
            }
            bytes_read += result;
        }

        printf("Reader received: %s\n", block);
    }

    close(fd);
    return 0;
}

// Commands to run the program (for viva demonstration):
// mkfifo myfifo                (or run HOL_Q01_c.c first)
// gcc HOL_Q01_d.c -o reader
// gcc HOL_Q01_e.c -o writer
// ./reader &                   -> starts and blocks, waiting for a writer
// ./writer                     -> sends 3 blocks; reader prints each as it arrives
