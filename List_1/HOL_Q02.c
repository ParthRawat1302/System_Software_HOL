#include <stdio.h>
#include <unistd.h>

int main() {

    printf("Process ID: %d\n", getpid());

    while (1) {
        // Infinite loop
    }

    return 0;
}

// Commands to run the program (for viva demonstration):

// gcc HOL_Q02.c -o infinite
// ./infinite &
// pgrep infinite
// cd /proc
// ls
// cd <PID>
// ls
// cat status
// cat cmdline
// cat comm
// cat stat
// cat statm
// ls -l exe
// ls -l cwd
// ls -l root
// ls -l fd
// cat environ | tr '\0' '\n'
// cd ~
// pkill infinite
// pgrep infinite
