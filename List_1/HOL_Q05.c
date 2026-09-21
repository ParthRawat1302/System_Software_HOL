#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main() {
    int fd[5];

    // Create and open five files
    fd[0] = creat("file1.txt", 0666);
    fd[1] = creat("file2.txt", 0666);
    fd[2] = creat("file3.txt", 0666);
    fd[3] = creat("file4.txt", 0666);
    fd[4] = creat("file5.txt", 0666);

    // Check whether files were created successfully
    for (int i = 0; i < 5; i++) {
        if (fd[i] == -1) {
            perror("Error creating file");
            return 1;
        }

        printf("file%d.txt -> File descriptor: %d\n", i + 1, fd[i]);
    }

    printf("Process ID: %d\n", getpid());

    // Infinite loop to keep files open
    while (1) {
    }

    return 0;
}


// Commands to run the program (for viva demonstration):
// gcc HOL_Q05.c -o five_files
// ./five_files &
// pgrep five_files
// ls -l /proc/<PID>/fd
// ls -l /proc/<PID>/fd/3
// ls -l /proc/<PID>/fd/4
// ls -l /proc/<PID>/fd/5
// ls -l /proc/<PID>/fd/6
// ls -l /proc/<PID>/fd/7
// ls -l
// kill <PID>
// pgrep five_files
// rm -f file1.txt file2.txt file3.txt file4.txt file5.txt
