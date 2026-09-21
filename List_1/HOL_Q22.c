#include <stdio.h>
#include <unistd.h>

int main() {
    pid_t pid = fork();

    if (pid == -1) {
        perror("fork");
        return 1;
    } else if (pid == 0) {
        printf("Child (pid=%d) exiting immediately.\n", getpid());
        _exit(0);
    } else {
        printf("Parent (pid=%d) sleeping without calling wait() - child becomes a zombie.\n", getpid());
        printf("In another terminal run: ps -el | grep Z\n");
        sleep(30);
    }

    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q22.c -o zombie
// ./zombie &
// ps -el | grep Z                     -> child shown with state 'Z' (zombie)
// ps -o pid,ppid,stat,cmd -C zombie
