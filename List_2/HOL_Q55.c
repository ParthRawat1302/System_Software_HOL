// Lowers RLIMIT_NOFILE (max open files) for this process and shows the effect.
#include <stdio.h>
#include <sys/resource.h>
#include <fcntl.h>

int main() {
    struct rlimit rl;

    getrlimit(RLIMIT_NOFILE, &rl);
    printf("Old RLIMIT_NOFILE soft limit: %ld\n", (long)rl.rlim_cur);

    rl.rlim_cur = 5; // must stay <= current hard limit
    if (setrlimit(RLIMIT_NOFILE, &rl) == -1) {
        perror("setrlimit");
        return 1;
    }

    getrlimit(RLIMIT_NOFILE, &rl);
    printf("New RLIMIT_NOFILE soft limit: %ld\n", (long)rl.rlim_cur);

    printf("Opening files until it fails (limit lowered to %ld)...\n", (long)rl.rlim_cur);
    int count = 0;
    while (1) {
        int fd = open("/dev/null", O_RDONLY);
        if (fd == -1) {
            perror("open failed as expected");
            break;
        }
        count++;
    }
    printf("Successfully opened %d additional files before hitting the limit.\n", count);

    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q55.c -o HOL_Q55
// ./HOL_Q55                  -> only 2 more files open (0, 1, 2 are stdin/out/err)
// ulimit -n                  -> the shell's limit is unchanged: setrlimit only
//                               affects the calling process and its children
