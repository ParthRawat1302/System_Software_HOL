// Creates an orphan by having the child kill the parent with SIGKILL.
#include <stdio.h>
#include <unistd.h>
#include <signal.h>

int main() {
    pid_t pid = fork();

    if (pid == -1) {
        perror("fork");
        return 1;
    } else if (pid == 0) {
        pid_t parent_pid = getppid();
        printf("Child (pid=%d) killing parent (pid=%d)\n", getpid(), parent_pid);
        kill(parent_pid, SIGKILL);

        sleep(2); // give init/systemd time to adopt this process
        printf("Child (pid=%d) is now orphaned. New parent pid=%d\n", getpid(), getppid());
    } else {
        printf("Parent (pid=%d) waiting to be killed by child (pid=%d)...\n", getpid(), pid);
        pause();
    }

    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q63.c -o HOL_Q63
// ./HOL_Q63                  -> the shell prints "Killed" for the parent; the child
//                               then reports a new parent pid (1 or a subreaper
//                               such as systemd --user)
// ps -o pid,ppid,cmd -C HOL_Q63   (run quickly, within 2 s, to see the new ppid)
