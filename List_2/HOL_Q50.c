// Deadlock using SysV semaphores.
// A set of two binary semaphores (sem 0 = resource R1, sem 1 = resource R2).
//   Parent: locks R1, sleeps, then asks for R2.
//   Child : locks R2, sleeps, then asks for R1.
// Each process holds one resource and waits forever for the other one, so all
// four deadlock conditions hold: mutual exclusion, hold-and-wait, no
// preemption and circular wait. Neither process can ever continue.
// Press Ctrl+C: the parent removes the semaphore set, which wakes the blocked
// semop() calls with EIDRM so both processes can exit cleanly.
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/sem.h>

union semun {                 // caller must define this (see man semctl)
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

int semid;

static void P(int sem_num, const char *who, const char *res) {
    struct sembuf op = {sem_num, -1, 0};
    printf("%s (pid=%d) waiting for %s...\n", who, getpid(), res);
    fflush(stdout);
    if (semop(semid, &op, 1) == -1) {
        // EIDRM/EINVAL: set was removed while waiting. EINTR: our own Ctrl+C
        // handler interrupted the wait (semop is never auto-restarted).
        if (errno == EIDRM || errno == EINVAL || errno == EINTR) {
            printf("%s (pid=%d): semaphore set removed, giving up on %s.\n", who, getpid(), res);
            exit(0);
        }
        perror("semop");
        exit(1);
    }
    printf("%s (pid=%d) acquired %s\n", who, getpid(), res);
    fflush(stdout);
}

static void cleanup(int sig) {
    semctl(semid, 0, IPC_RMID); // wakes both blocked processes with EIDRM
}

int main() {
    key_t key = ftok("HOL_Q50.c", 50);
    semid = semget(key, 2, 0666 | IPC_CREAT);
    if (semid == -1) {
        perror("semget");
        return 1;
    }

    unsigned short init[2] = {1, 1}; // both resources free
    union semun arg;
    arg.array = init;
    if (semctl(semid, 0, SETALL, arg) == -1) {
        perror("semctl SETALL");
        return 1;
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        signal(SIGINT, SIG_IGN); // let the parent handle Ctrl+C and clean up
        P(1, "Child ", "R2");
        sleep(2);                // make sure the parent grabs R1 meanwhile
        P(0, "Child ", "R1");    // blocks forever: parent holds R1
        printf("Child finished (never printed while deadlocked).\n");
        exit(0);
    }

    signal(SIGINT, cleanup);
    P(0, "Parent", "R1");
    sleep(2);                    // make sure the child grabs R2 meanwhile
    printf("\n*** DEADLOCK: parent holds R1 and wants R2, child holds R2 and wants R1 ***\n");
    printf("*** Press Ctrl+C to break it and remove the semaphores. ***\n\n");
    fflush(stdout);
    P(1, "Parent", "R2");        // blocks forever: child holds R2
    printf("Parent finished (never printed while deadlocked).\n");

    wait(NULL);
    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q50.c -o HOL_Q50
// ./HOL_Q50
// Both processes print "waiting for ..." and then nothing more: deadlock.
// In another terminal:
//   ps -o pid,ppid,stat,wchan:20,cmd -C HOL_Q50   -> both in state S, stuck in semop
//   ipcs -s -i <semid>                            -> semval 0 on both semaphores
// Back in the first terminal press Ctrl+C: the semaphores are removed and both exit.
// ipcs -s                                         -> the set is gone
