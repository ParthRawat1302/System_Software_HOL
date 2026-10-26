// Create a FIFO using the mknod() system call
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>

int main() {
    if (mknod("myfifo_mknod", S_IFIFO | 0666, 0) == -1) {
        perror("mknod");
        return 1;
    }

    printf("FIFO 'myfifo_mknod' created using mknod().\n");
    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q36_d.c -o HOL_Q36_d
// ./HOL_Q36_d
// ls -l myfifo_mknod         -> first character 'p' means it is a FIFO
// rm myfifo_mknod            (a second run fails with "File exists" until removed)
// Parts a, b and c (shell commands and strace) are in HOL_Q36_notes.txt
