#include <stdio.h>
#include <sys/ipc.h>
#include <sys/msg.h>

int main() {
    key_t key = ftok("HOL_Q41.c", 65); // same key as HOL_Q41.c -> same queue
    int msgid = msgget(key, 0666 | IPC_CREAT);
    if (msgid == -1) {
        perror("msgget");
        return 1;
    }

    if (msgctl(msgid, IPC_RMID, NULL) == -1) {
        perror("msgctl IPC_RMID");
        return 1;
    }

    printf("Message queue %d removed.\n", msgid);
    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q41.c -o HOL_Q41 && ./HOL_Q41
// ipcs -q                    -> queue is listed
// gcc HOL_Q46.c -o HOL_Q46
// ./HOL_Q46
// ipcs -q                    -> queue is gone
// (shell equivalent: ipcrm -q <msqid>)
