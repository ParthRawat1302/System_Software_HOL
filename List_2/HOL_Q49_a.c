// Re-implements the ticket-number creation program (List-1 Q.17) using a
// binary semaphore instead of fcntl record locking to protect ticket.txt.
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "HOL_Q49_sem_helper.h"

int main() {
    key_t key = ftok("HOL_Q49_a.c", 90);
    int semid = get_sem(key, 1); // binary semaphore, initialised to 1 only on creation
    if (semid == -1) {
        perror("semget");
        return 1;
    }

    int fd = open("ticket_sem.txt", O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
        perror("open");
        return 1;
    }

    sem_wait(semid, 0); // critical section start

    char buf[16] = {0};
    lseek(fd, 0, SEEK_SET);
    int n = read(fd, buf, sizeof(buf) - 1);
    buf[n] = '\0';
    int ticket = (n > 0) ? atoi(buf) : 100;
    if (n > 0) ticket++;

    char newbuf[16];
    int len = sprintf(newbuf, "%d", ticket);
    lseek(fd, 0, SEEK_SET);
    ftruncate(fd, 0);
    write(fd, newbuf, len);

    printf("New ticket number: %d\n", ticket);

    sem_signal(semid, 0); // critical section end

    close(fd);
    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q49_a.c -o HOL_Q49_a
// ./HOL_Q49_a                -> first run creates ticket_sem.txt with 100
// ./HOL_Q49_a                -> 101
// Run several at once; the semaphore serialises them, so no update is lost:
// for i in 1 2 3 4 5; do ./HOL_Q49_a & done; wait
// cat ticket_sem.txt         -> increased by exactly 5
// ipcs -s                    -> the semaphore is listed
