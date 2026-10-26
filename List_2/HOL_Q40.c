#define _GNU_SOURCE // needed for F_GETPIPE_SZ, a Linux-specific fcntl extension
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

int main() {
    printf("Max files per process (OPEN_MAX): %ld\n", sysconf(_SC_OPEN_MAX));

    int fd[2];
    pipe(fd);

    long pipe_size = fcntl(fd[1], F_GETPIPE_SZ);
    printf("Pipe size (circular buffer): %ld bytes\n", pipe_size);

    close(fd[0]);
    close(fd[1]);
    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q40.c -o HOL_Q40
// ./HOL_Q40
// ulimit -n                        -> compare with the max open files value
// cat /proc/sys/fs/pipe-max-size   -> upper limit a pipe can be resized to
//                                    (default pipe size is usually 65536 bytes)
