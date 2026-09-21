#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main() {
    int fd;

    // Open existing file in read-write mode
    fd = open("existing.txt", O_RDWR);

    if (fd == -1) {
        perror("Error opening existing file");
        return 1;
    }

    printf("File opened successfully in read-write mode.\n");
    printf("File descriptor: %d\n", fd);

    close(fd);

    // Try O_EXCL with O_CREAT
    fd = open("existing.txt", O_RDWR | O_CREAT | O_EXCL, 0666);

    if (fd == -1) {
        perror("O_EXCL test");
    } else {
        printf("File created successfully with O_EXCL.\n");
        printf("File descriptor: %d\n", fd);
        close(fd);
    }

    return 0;
}

// Commands to run the program (for viva demonstration):
// echo "This is an existing file." > existing.txt
// gcc HOL_Q04.c -o open_file
// ./open_file
// -> second open() with O_CREAT|O_EXCL fails with "File exists" since
//    existing.txt is already present - that is the whole point of O_EXCL.
//
// rm -f existing.txt
// ./open_file      -> now the first open() fails ("No such file or directory")
//                     and the O_EXCL create succeeds instead
// ls -l existing.txt
