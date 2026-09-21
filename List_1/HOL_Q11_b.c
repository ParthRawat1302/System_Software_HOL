#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main() {
    int fd1, fd2;

    fd1 = open("dup2_test.txt", O_CREAT | O_WRONLY | O_APPEND, 0666);
    if (fd1 == -1) {
        perror("Error opening file");
        return 1;
    }

    fd2 = open("temp.txt", O_CREAT | O_WRONLY, 0666);
    if (fd2 == -1) {
        perror("Error opening temp file");
        return 1;
    }

    // dup2 closes fd2 and makes it point to the same open file as fd1
    if (dup2(fd1, fd2) == -1) {
        perror("Error in dup2");
        return 1;
    }

    write(fd1, "Written using fd1\n", 19);
    write(fd2, "Written using fd2 (dup2 of fd1)\n", 33);

    close(fd1);
    close(fd2);

    printf("Check dup2_test.txt - both writes should appear one after another.\n");
    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q11_b.c -o dup2_demo
// ./dup2_demo
// cat dup2_test.txt
