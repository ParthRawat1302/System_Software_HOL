// Protects a shared memory segment from concurrent write access using a
// binary semaphore. Run two instances at once to see the writes serialize.
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/shm.h>
#include "HOL_Q49_sem_helper.h"

int main() {
    key_t shm_key = ftok("HOL_Q49_b.c", 91);
    key_t sem_key = ftok("HOL_Q49_b.c", 92);

    int shmid = shmget(shm_key, 1024, 0666 | IPC_CREAT);
    int semid = get_sem(sem_key, 1); // binary semaphore guarding the segment
    if (shmid == -1 || semid == -1) {
        perror("shmget/semget");
        return 1;
    }

    char *data = shmat(shmid, NULL, 0);
    if (data == (char *)-1) {
        perror("shmat");
        return 1;
    }

    sem_wait(semid, 0);

    printf("PID %d entering critical section...\n", getpid());
    sprintf(data, "Last written by PID %d", getpid());
    sleep(3); // hold the section to make the serialization visible
    printf("PID %d wrote: %s\n", getpid(), data);

    sem_signal(semid, 0);

    shmdt(data);
    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q49_b.c -o HOL_Q49_b
// ./HOL_Q49_b & ./HOL_Q49_b & wait
// Expected: the second process enters the critical section only after the
// first one leaves (about 3 s later); the writes never overlap.
// ipcs -m ; ipcs -s          -> the segment and the semaphore are listed
