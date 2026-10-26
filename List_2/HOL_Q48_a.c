// Create and initialize a binary semaphore (value 0 or 1) using SysV semaphores
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/sem.h>

union semun {                 // caller must define this (see man semctl)
    int val;                  // value for SETVAL
    struct semid_ds *buf;     // buffer for IPC_STAT, IPC_SET
    unsigned short *array;    // array for GETALL, SETALL
};

int main() {
    key_t key = ftok("HOL_Q48_a.c", 85);

    int semid = semget(key, 1, 0666 | IPC_CREAT);
    if (semid == -1) {
        perror("semget");
        return 1;
    }

    union semun arg;
    arg.val = 1; // binary semaphore, initially "unlocked"

    if (semctl(semid, 0, SETVAL, arg) == -1) {
        perror("semctl SETVAL");
        return 1;
    }

    printf("Binary semaphore created (semid=%d) and initialized to %d.\n", semid, arg.val);
    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q48_a.c -o HOL_Q48_a
// ./HOL_Q48_a
// ipcs -s                    -> semaphore set listed (nsems = 1)
// ipcs -s -i <semid>         -> semval = 1 (binary semaphore)
