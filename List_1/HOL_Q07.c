#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main() {
    int source, destination;
    char buffer[1024];
    int n;

    // Open file1 for reading
    source = open("file1", O_RDONLY);

    if (source == -1) {
        perror("Error opening file1");
        return 1;
    }

    // Create file2 for writing
    destination = open("file2", O_WRONLY | O_CREAT | O_TRUNC, 0666);

    if (destination == -1) {
        perror("Error creating file2");
        close(source);
        return 1;
    }

    // Copy contents from file1 to file2
    while ((n = read(source, buffer, sizeof(buffer))) > 0) {
        write(destination, buffer, n);
    }

    close(source);
    close(destination);

    printf("File copied successfully.\n");

    return 0;
}

// Commands to run the program (for viva demonstration):
// echo "sample content for copy" > file1
// gcc HOL_Q07.c -o copy_file
// ./copy_file
// cat file2
// diff file1 file2       -> no output means the copy is byte-identical
//
// Equivalent shell command being replicated: cp file1 file2
