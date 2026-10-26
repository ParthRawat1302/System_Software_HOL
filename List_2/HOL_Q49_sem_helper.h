// Small helper shared by the Q49 programs for P() (wait/decrement) and
// V() (signal/increment) operations on a SysV semaphore.
#ifndef HOL_Q49_SEM_HELPER_H
#define HOL_Q49_SEM_HELPER_H

#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>

union semun {                 // caller must define this (see man semctl)
    int val;                  // value for SETVAL
    struct semid_ds *buf;     // buffer for IPC_STAT, IPC_SET
    unsigned short *array;    // array for GETALL, SETALL
};

// Returns the semaphore for `key`. Only the process that actually CREATES it
// (IPC_EXCL succeeds) sets the initial value. Re-initialising on every run
// would reset the count while another process is inside the critical section
// and break mutual exclusion.
static int get_sem(key_t key, int init_val) {
    int semid = semget(key, 1, 0666 | IPC_CREAT | IPC_EXCL);
    if (semid != -1) {
        union semun arg;
        arg.val = init_val;
        if (semctl(semid, 0, SETVAL, arg) == -1) return -1;
        return semid;
    }
    if (errno != EEXIST) return -1;
    return semget(key, 1, 0666); // already exists: just open it
}

static int sem_op(int semid, int sem_num, int change) {
    struct sembuf op;
    op.sem_num = sem_num;
    op.sem_op = change;
    op.sem_flg = SEM_UNDO; // kernel undoes the op if the process dies inside the section
    return semop(semid, &op, 1);
}

#define sem_wait(semid, n)   sem_op((semid), (n), -1)   // P()
#define sem_signal(semid, n) sem_op((semid), (n), 1)    // V()

#endif
