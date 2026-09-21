#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main() {
    int fd, flags;

    fd = open("input.txt", O_RDWR);
    if (fd == -1) {
        perror("Error opening file");
        return 1;
    }

    flags = fcntl(fd, F_GETFL);
    if (flags == -1) {
        perror("Error in fcntl F_GETFL");
        return 1;
    }

    int mode = flags & O_ACCMODE;
    if (mode == O_RDONLY) printf("File opened in O_RDONLY mode\n");
    else if (mode == O_WRONLY) printf("File opened in O_WRONLY mode\n");
    else if (mode == O_RDWR) printf("File opened in O_RDWR mode\n");

    close(fd);
    return 0;
}

// Commands to run the program (for viva demonstration):
// echo "sample data" > input.txt
// gcc HOL_Q12.c -o fcntl_mode
// ./fcntl_mode      -> prints "File opened in O_RDWR mode"
