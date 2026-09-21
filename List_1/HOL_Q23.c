#include <stdio.h>
#include <unistd.h>

int main() {
    pid_t pid = fork();

    if (pid == -1) {
        perror("fork");
        return 1;
    } else if (pid == 0) {
        sleep(2); // give the parent time to exit first
        printf("Child (pid=%d) is now orphaned. New parent pid=%d\n", getpid(), getppid());
    } else {
        printf("Parent (pid=%d) exiting immediately.\n", getpid());
        _exit(0);
    }

    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q23.c -o orphan
// ./orphan
// -> child prints its new parent pid after the original parent exits;
//    on Linux it is reparented to the nearest subreaper (often pid 1 or
//    the systemd user manager instead of the shell's original pid)
// ps -o pid,ppid,cmd -p <printed-child-pid>   (run quickly before it exits)
