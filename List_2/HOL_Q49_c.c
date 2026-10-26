// Protects two pseudo-resources using a counting semaphore (value 2), so at
// most two processes can hold a resource at the same time.
#include <stdio.h>
#include <unistd.h>
#include "HOL_Q49_sem_helper.h"

int main() {
    key_t key = ftok("HOL_Q49_c.c", 93);
    int semid = get_sem(key, 2); // counting semaphore: two pseudo-resources available
    if (semid == -1) {
        perror("semget");
        return 1;
    }

    printf("PID %d requesting a resource...\n", getpid());
    sem_wait(semid, 0);

    printf("PID %d acquired a resource. Using it for 5 seconds...\n", getpid());
    sleep(5);

    printf("PID %d releasing the resource.\n", getpid());
    sem_signal(semid, 0);

    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q49_c.c -o HOL_Q49_c
// for i in 1 2 3 4; do ./HOL_Q49_c & done; wait
// Expected: only two processes "acquire a resource" at a time; the other two
// wait until one is released (counting semaphore with value 2).
