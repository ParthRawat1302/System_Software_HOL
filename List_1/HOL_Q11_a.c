#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main() {
    int fd1, fd2;

    fd1 = open("dup_test.txt", O_CREAT | O_WRONLY | O_APPEND, 0666);
    if (fd1 == -1) {
        perror("Error opening file");
        return 1;
    }

    // dup() returns the lowest unused fd, sharing the same file offset as fd1
    fd2 = dup(fd1);
    if (fd2 == -1) {
        perror("Error duplicating fd");
        return 1;
    }

    write(fd1, "Written using fd1\n", 19);
    write(fd2, "Written using fd2 (dup of fd1)\n", 32);

    close(fd1);
    close(fd2);

    printf("Check dup_test.txt - both writes should appear one after another.\n");
    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q11_a.c -o dup_demo
// ./dup_demo
// cat dup_test.txt
