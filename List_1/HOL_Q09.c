#include <stdio.h>
#include <sys/stat.h>
#include <time.h>

int main() {
    struct stat file_info;

    if (stat("input.txt", &file_info) == -1) {
        perror("Error getting file information");
        return 1;
    }

    printf("File information:\n");
    printf("-----------------------------\n");
    printf("Inode number       : %lu\n", (unsigned long)file_info.st_ino);
    printf("Hard links         : %lu\n", (unsigned long)file_info.st_nlink);

    printf("UID                : %u\n", file_info.st_uid);
    printf("GID                : %u\n", file_info.st_gid);
    printf("Block size         : %ld bytes\n", file_info.st_blksize);
    printf("Number of blocks   : %ld\n", file_info.st_blocks);

    printf("Size               : %ld bytes\n", file_info.st_size);

    printf("Last access        : %s", ctime(&file_info.st_atime));
    printf("Last modification  : %s", ctime(&file_info.st_mtime));
    printf("Last change        : %s", ctime(&file_info.st_ctime));

    return 0;
}

// Commands to run the program (for viva demonstration):
// echo "sample data for stat" > input.txt
// gcc HOL_Q09.c -o file_info
// ./file_info
//
// Cross-check the printed values against:
// stat input.txt
// ls -li input.txt
