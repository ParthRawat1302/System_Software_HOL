#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main() {
    int fd;

    fd = creat("myfile.txt", 0666);

    if (fd == -1) {
        perror("Error creating file");
        return 1;
    }

    printf("File created successfully.\n");
    printf("File descriptor: %d\n", fd);

    close(fd);

    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q03.c -o create_file
// ./create_file
// ls -l myfile.txt
// stat myfile.txt
