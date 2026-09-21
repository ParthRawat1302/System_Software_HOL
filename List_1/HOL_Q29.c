#include <stdio.h>
#include <sched.h>
#include <unistd.h>

int main() {
    int policy = sched_getscheduler(0);

    printf("Current scheduling policy: ");
    switch (policy) {
        case SCHED_OTHER: printf("SCHED_OTHER\n"); break;
        case SCHED_FIFO:  printf("SCHED_FIFO\n"); break;
        case SCHED_RR:    printf("SCHED_RR\n"); break;
        default:          printf("Unknown (%d)\n", policy);
    }

    struct sched_param param;
    param.sched_priority = sched_get_priority_min(SCHED_RR);

    // Setting a real-time policy usually requires root/CAP_SYS_NICE
    if (sched_setscheduler(0, SCHED_RR, &param) == -1) {
        perror("sched_setscheduler (try running with sudo)");
        return 1;
    }

    policy = sched_getscheduler(0);
    printf("New scheduling policy: %s\n", policy == SCHED_RR ? "SCHED_RR" : "unknown");

    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q29.c -o sched_policy
// ./sched_policy          -> sched_setscheduler() fails: "Operation not permitted"
// sudo ./sched_policy     -> succeeds; policy changes from SCHED_OTHER to SCHED_RR
