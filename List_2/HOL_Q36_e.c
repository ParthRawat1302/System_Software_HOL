// Create a FIFO using the mkfifo() library function
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>

int main() {
    if (mkfifo("myfifo_lib", 0666) == -1) {
        perror("mkfifo");
        return 1;
    }

    printf("FIFO 'myfifo_lib' created using mkfifo().\n");
    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q36_e.c -o HOL_Q36_e
// ./HOL_Q36_e
// ls -l myfifo_lib           -> first character 'p' means it is a FIFO
// rm myfifo_lib              (a second run fails with "File exists" until removed)
// Parts a, b and c (shell commands and strace) are in HOL_Q36_notes.txt
