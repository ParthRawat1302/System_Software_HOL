// Attach the same shared memory read-only and confirm a write attempt fails.
// Run HOL_Q47_a.c first so the segment exists and has data.
#include <stdio.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>

int main() {
    key_t key = ftok("HOL_Q47_a.c", 75);

    int shmid = shmget(key, 1024, 0666);
    if (shmid == -1) {
        perror("shmget (run HOL_Q47_a.c first)");
        return 1;
    }

    char *data = shmat(shmid, NULL, SHM_RDONLY);
    if (data == (char *)-1) {
        perror("shmat");
        return 1;
    }

    printf("Read from shared memory: %s\n", data);

    printf("Attempting to write to a SHM_RDONLY attachment (this will crash with SIGSEGV)...\n");
    strcpy(data, "trying to overwrite"); // triggers SIGSEGV: segment is read-only

    shmdt(data);
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
