// Removes the semaphores created by HOL_Q49_a.c, HOL_Q49_b.c and HOL_Q49_c.c.
// Each key must be built with the same file + project id used to create it.
#include <stdio.h>
#include "HOL_Q49_sem_helper.h"

static void remove_sem(const char *file, int proj_id) {
    key_t key = ftok(file, proj_id);
    int semid = semget(key, 1, 0666);
    if (semid == -1) {
        printf("%s: no semaphore to remove\n", file);
        return;
    }
    if (semctl(semid, 0, IPC_RMID) == -1) {
        perror("semctl IPC_RMID");
        return;
    }
    printf("%s: semaphore %d removed.\n", file, semid);
}

int main() {
    remove_sem("HOL_Q49_a.c", 90); // ticket semaphore
    remove_sem("HOL_Q49_b.c", 92); // shared-memory semaphore
    remove_sem("HOL_Q49_c.c", 93); // counting semaphore
    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q49_d.c -o HOL_Q49_d
// ipcs -s                    -> semaphores created by HOL_Q49_a/b/c are listed
// ./HOL_Q49_d
// ipcs -s                    -> they are gone
// ipcrm -m <shmid>           (also remove the shared memory used by HOL_Q49_b)
