// shell command
// mkfifo myfifo
// ls -l myfifo

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

int main() {

    // Create FIFO
    if (mkfifo("myfifo", 0666) == -1) {
        perror("Error creating FIFO");
        return 1;
    }

    printf("FIFO created successfully.\n");

    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q01_c.c -o fifo_demo
// ./fifo_demo
// ls -l myfifo             -> file type 'p' (FIFO/named pipe)
// file myfifo
//
// To actually exchange data through it, use HOL_Q01_e.c (writer) and HOL_Q01_d.c (reader):
// gcc HOL_Q01_e.c -o writer
// gcc HOL_Q01_d.c -o reader
// ./reader &     (start the reader first - it blocks until a writer opens the FIFO)
// ./writer
