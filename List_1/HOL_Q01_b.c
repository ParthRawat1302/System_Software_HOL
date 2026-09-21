#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main() {
    int fd;

    // Create the original file
    fd = open("original_hard.txt", O_CREAT | O_WRONLY, 0666);

    if (fd == -1) {
        perror("Error creating original file");
        return 1;
    }

    write(fd, "Hello from hard link file\n", 26);
    close(fd);

    // Create the hard link
    if (link("original_hard.txt", "hardlink.txt") == -1) {
        perror("Error creating hard link");
        return 1;
    }

    printf("Hard link created successfully.\n");

    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q01_b.c -o hardlink_demo
// ./hardlink_demo
// ls -li original_hard.txt hardlink.txt   -> same inode number, link count = 2
// cat hardlink.txt
//
// Equivalent using only the shell command (part i of the question):
// ln original_hard.txt hardlink_shell.txt
// ls -li hardlink_shell.txt
