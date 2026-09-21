#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main() {
    int fd;
    char ch;
    char line[1024];
    int i = 0;
    int n;

    // Open file in read-only mode
    fd = open("input.txt", O_RDONLY);

    if (fd == -1) {
        perror("Error opening file");
        return 1;
    }

    // Read file character by character
    while ((n = read(fd, &ch, 1)) > 0) {

        line[i++] = ch;

        // If newline is reached, display the line
        if (ch == '\n') {
            write(STDOUT_FILENO, line, i);
            i = 0;
        }
    }

    // Display last line if it does not end with newline
    if (i > 0) {
        write(STDOUT_FILENO, line, i);
    }

    // Close file at EOF
    close(fd);

    return 0;
}

// Commands to run the program (for viva demonstration):
// printf "line one\nline two\nline three\n" > input.txt
// gcc HOL_Q08.c -o read_lines
// ./read_lines
