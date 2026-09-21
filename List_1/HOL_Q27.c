#include <stdio.h>
#include <sched.h>

int main() {
    int max_prio = sched_get_priority_max(SCHED_FIFO);
    int min_prio = sched_get_priority_min(SCHED_FIFO);
    printf("SCHED_FIFO priority range: min=%d, max=%d\n", min_prio, max_prio);

    max_prio = sched_get_priority_max(SCHED_RR);
    min_prio = sched_get_priority_min(SCHED_RR);
    printf("SCHED_RR priority range: min=%d, max=%d\n", min_prio, max_prio);

    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q27.c -o rt_priority
// ./rt_priority
// chrt -m           -> shell command showing the same min/max priority ranges
