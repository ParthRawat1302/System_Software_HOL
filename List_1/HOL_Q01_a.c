#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main() {
    int fd;

    // Create the original file
    fd = open("original.txt", O_CREAT | O_WRONLY, 0666);

    if (fd == -1) {
        perror("Error creating original file");
        return 1;
    }

    write(fd, "Hello from original file\n", 25);
    close(fd);

    // Create the soft link
    if (symlink("original.txt", "softlink.txt") == -1) {
        perror("Error creating soft link");
        return 1;
    }

    printf("Soft link created successfully.\n");

    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q01_a.c -o softlink_demo
// ./softlink_demo
// ls -l softlink.txt original.txt    -> softlink.txt shows 'l' and "-> original.txt"
// cat softlink.txt
// stat softlink.txt                  -> "File type: symbolic link"
//
// Equivalent using only the shell command (part i of the question):
// ln -s original.txt softlink_shell.txt
// ls -l softlink_shell.txt
