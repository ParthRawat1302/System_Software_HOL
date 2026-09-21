// The fd is opened before fork(), so parent and child share the same open
// file description (and file offset). With O_APPEND, both writes land in the
// file without overwriting each other.
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main() {
    int fd = open("shared_output.txt", O_CREAT | O_WRONLY | O_APPEND, 0666);
    if (fd == -1) {
        perror("open");
        return 1;
    }

    pid_t pid = fork();

    if (pid == -1) {
        perror("fork");
        return 1;
    } else if (pid == 0) {
        write(fd, "Written by child\n", 18);
    } else {
        write(fd, "Written by parent\n", 19);
    }

    close(fd);
    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q21.c -o fork_write_file
// ./fork_write_file
// cat shared_output.txt   -> both "Written by parent" and "Written by child" appear
