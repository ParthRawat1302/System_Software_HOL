// Runs a script once a target time is reached, as a background daemon process.
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <syslog.h>

int main() {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    }
    if (pid > 0) {
        exit(0); // parent exits, child continues in the background
    }

    if (setsid() == -1) { // detach from the controlling terminal
        exit(1);
    }

    umask(0);
    chdir("/");

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    openlog("mydaemon", LOG_PID, LOG_DAEMON);

    time_t target = time(NULL) + 30; // run 30 seconds after the daemon starts

    while (1) {
        if (time(NULL) >= target) {
            syslog(LOG_INFO, "Running scheduled script now");
            system("/bin/echo Scheduled task executed >> /tmp/daemon_output.txt");
            break;
        }
        sleep(1);
    }

    closelog();
    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q30.c -o daemon
// ./daemon
// ps -ef | grep daemon         -> parent pid is 1 (or the systemd user manager),
//                                 confirming it detached into the background
// (wait ~30 seconds for the target time to be reached)
// cat /tmp/daemon_output.txt
// journalctl -t mydaemon       (or: grep mydaemon /var/log/syslog)
