#include <stdio.h>
#include <unistd.h>
#include <limits.h>

int main() {
    printf("Max length of exec() arguments (ARG_MAX): %ld\n", sysconf(_SC_ARG_MAX));
    printf("Max simultaneous processes per user (CHILD_MAX): %ld\n", sysconf(_SC_CHILD_MAX));
    printf("Clock ticks per second (jiffy, CLK_TCK): %ld\n", sysconf(_SC_CLK_TCK));
    printf("Max number of open files (OPEN_MAX): %ld\n", sysconf(_SC_OPEN_MAX));
    printf("Page size (PAGESIZE): %ld bytes\n", sysconf(_SC_PAGESIZE));
    printf("Total physical pages (PHYS_PAGES): %ld\n", sysconf(_SC_PHYS_PAGES));
    printf("Currently available physical pages (AVPHYS_PAGES): %ld\n", sysconf(_SC_AVPHYS_PAGES));

    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q57.c -o HOL_Q57
// ./HOL_Q57
// Compare with the shell:
// getconf ARG_MAX ; getconf CHILD_MAX ; getconf CLK_TCK ; getconf OPEN_MAX
// getconf PAGESIZE ; getconf _PHYS_PAGES ; getconf _AVPHYS_PAGES
