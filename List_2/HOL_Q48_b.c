// Create and initialize a counting semaphore (value > 1) using SysV semaphores
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/sem.h>

union semun {                 // caller must define this (see man semctl)
    int val;                  // value for SETVAL
    struct semid_ds *buf;     // buffer for IPC_STAT, IPC_SET
    unsigned short *array;    // array for GETALL, SETALL
};

int main() {
    key_t key = ftok("HOL_Q48_b.c", 86);

    int semid = semget(key, 1, 0666 | IPC_CREAT);
    if (semid == -1) {
        perror("semget");
        return 1;
    }

    union semun arg;
    arg.val = 3; // counting semaphore allowing up to 3 concurrent resources

    if (semctl(semid, 0, SETVAL, arg) == -1) {
        perror("semctl SETVAL");
        return 1;
    }

    printf("Counting semaphore created (semid=%d) and initialized to %d.\n", semid, arg.val);
    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q48_b.c -o HOL_Q48_b
// ./HOL_Q48_b
// ipcs -s                    -> semaphore set listed (nsems = 1)
// ipcs -s -i <semid>         -> semval = 3 (counting semaphore)
