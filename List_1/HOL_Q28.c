#include <stdio.h>
#include <sys/resource.h>
#include <unistd.h>
#include <errno.h>

int main() {
    errno = 0;
    // getpriority can legitimately return -1, so errno must be checked too
    int prio = getpriority(PRIO_PROCESS, 0);
    if (prio == -1 && errno != 0) {
        perror("getpriority");
        return 1;
    }

    printf("Current priority (nice value) of PID %d: %d\n", getpid(), prio);
    printf("Re-run with, e.g.: nice -n 10 ./a.out   to see the priority change.\n");
    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q28.c -o priority
// ./priority                    -> default niceness, usually 0
// nice -n 10 ./priority         -> niceness now 10 (lower priority)
// sudo nice -n -5 ./priority    -> negative niceness needs root privilege
// ps -o pid,ni,cmd -p $$        -> shows the current shell's own niceness
