#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define FIFO_NAME "myfifo"
#define BLOCK_SIZE 32

int main(void) {
    const char *blocks[] = {
        "Block 1: first message",
        "Block 2: second message",
        "Block 3: third message"
    };
    int fd = open(FIFO_NAME, O_WRONLY);

    if (fd == -1) {
        perror("open");
        return 1;
    }

    for (int i = 0; i < 3; i++) {
        char block[BLOCK_SIZE] = {0};
        strncpy(block, blocks[i], BLOCK_SIZE - 1);

        if (write(fd, block, BLOCK_SIZE) != BLOCK_SIZE) {
            perror("write");
            close(fd);
            return 1;
        }
        printf("Writer sent: %s\n", block);
    }

    close(fd);
    return 0;
}

// Commands to run the program (for viva demonstration):
// mkfifo myfifo                (or run HOL_Q01_c.c first)
// gcc HOL_Q01_d.c -o reader
// gcc HOL_Q01_e.c -o writer
// ./reader &                   -> start the reader first, it waits for data
// ./writer                     -> sends 3 blocks of text through the FIFO
