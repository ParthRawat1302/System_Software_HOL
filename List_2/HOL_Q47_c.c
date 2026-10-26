// Detach a shared memory segment from this process's address space
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>

int main() {
    key_t key = ftok("HOL_Q47_a.c", 75);

    int shmid = shmget(key, 1024, 0666);
    if (shmid == -1) {
        perror("shmget (run HOL_Q47_a.c first)");
        return 1;
    }

    char *data = shmat(shmid, NULL, 0);
    if (data == (char *)-1) {
        perror("shmat");
        return 1;
    }

    printf("Attached. Data: %s\n", data);

    if (shmdt(data) == -1) {
        perror("shmdt");
        return 1;
    }

    printf("Shared memory detached successfully.\n");
    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q47_a.c -o HOL_Q47_a
// gcc HOL_Q47_b.c -o HOL_Q47_b
// gcc HOL_Q47_c.c -o HOL_Q47_c
// gcc HOL_Q47_d.c -o HOL_Q47_d
// ./HOL_Q47_a    (a) create the segment and write data
// ipcs -m        -> segment listed
// ./HOL_Q47_b    (b) reads the data, then the write crashes with
//                    "Segmentation fault": a SHM_RDONLY attachment cannot be overwritten
// ./HOL_Q47_c    (c) attach and detach; "nattch" in ipcs -m returns to 0
// ./HOL_Q47_d    (d) remove the segment
// ipcs -m        -> segment is gone
