#include <stdio.h>
#include <sys/stat.h>

int main(int argc, char *argv[]) {
    struct stat st;

    if (argc != 2) {
        printf("Usage: %s <filename>\n", argv[0]);
        return 1;
    }

    // lstat (not stat) so symlinks are reported as links, not the target type
    if (lstat(argv[1], &st) == -1) {
        perror("lstat error");
        return 1;
    }

    printf("File: %s\nType: ", argv[1]);

    if (S_ISREG(st.st_mode)) printf("Regular file\n");
    else if (S_ISDIR(st.st_mode)) printf("Directory\n");
    else if (S_ISCHR(st.st_mode)) printf("Character device\n");
    else if (S_ISBLK(st.st_mode)) printf("Block device\n");
    else if (S_ISFIFO(st.st_mode)) printf("FIFO/pipe\n");
    else if (S_ISLNK(st.st_mode)) printf("Symbolic link\n");
    else if (S_ISSOCK(st.st_mode)) printf("Socket\n");
    else printf("Unknown\n");

    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q14.c -o file_type
// ./file_type HOL_Q14.c            -> Regular file
// ./file_type .                    -> Directory
// ln -s HOL_Q14.c mylink
// ./file_type mylink               -> Symbolic link
// mkfifo myfifo_test
// ./file_type myfifo_test          -> FIFO/pipe
// ./file_type /dev/tty             -> Character device
// ls -l /dev/sda 2>/dev/null && ./file_type /dev/sda   (Block device, if present)
