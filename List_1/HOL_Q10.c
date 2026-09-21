#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main() {
    int fd;
    off_t position;

    // Open file in read-write mode
    fd = open("lseek_file.txt", O_RDWR | O_CREAT | O_TRUNC, 0666);

    if (fd == -1) {
        perror("Error opening file");
        return 1;
    }

    // Write first 10 bytes
    if (write(fd, "ABCDEFGHIJ", 10) != 10) {
        perror("Error writing first 10 bytes");
        close(fd);
        return 1;
    }

    // Move file pointer 10 bytes forward
    position = lseek(fd, 10, SEEK_CUR);

    // Check return value of lseek
    if (position == (off_t)-1) {
        perror("lseek failed");
        close(fd);
        return 1;
    }

    printf("File pointer position after lseek: %ld\n", (long)position);

    // Write another 10 bytes
    if (write(fd, "1234567890", 10) != 10) {
        perror("Error writing second 10 bytes");
        close(fd);
        return 1;
    }

    close(fd);

    printf("Data written successfully.\n");

    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q10.c -o lseek_demo
// ./lseek_demo
// od -c lseek_file.txt    -> ABCDEFGHIJ, 10 NUL (\0) bytes, then 1234567890
// ls -l lseek_file.txt    -> size is 30 bytes even though only 20 were written
