#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main() {
    int fd1, fd2;

    fd1 = open("fcntl_test.txt", O_CREAT | O_WRONLY | O_APPEND, 0666);
    if (fd1 == -1) {
        perror("Error opening file");
        return 1;
    }

    // F_DUPFD duplicates fd1 using the lowest available descriptor >= 0
    fd2 = fcntl(fd1, F_DUPFD, 0);
    if (fd2 == -1) {
        perror("Error in fcntl F_DUPFD");
        return 1;
    }

    write(fd1, "Written using fd1\n", 19);
    write(fd2, "Written using fd2 (fcntl dup of fd1)\n", 38);

    close(fd1);
    close(fd2);

    printf("Check fcntl_test.txt - both writes should appear one after another.\n");
    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q11_c.c -o fcntl_dup_demo
// ./fcntl_dup_demo
// cat fcntl_test.txt
