// Receive from the message queue with IPC_NOWAIT (returns immediately if empty)
#include <stdio.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/msg.h>

struct message {
    long mtype;
    char mtext[100];
};

int main() {
    key_t key = ftok("HOL_Q41.c", 65); // same key as HOL_Q41.c -> same queue
    int msgid = msgget(key, 0666 | IPC_CREAT);
    if (msgid == -1) {
        perror("msgget");
        return 1;
    }

    struct message msg;

    if (msgrcv(msgid, &msg, sizeof(msg.mtext), 1, IPC_NOWAIT) == -1) {
        if (errno == ENOMSG) {
            printf("No message available right now (IPC_NOWAIT).\n");
        } else {
            perror("msgrcv");
        }
        return 1;
    }

    printf("Received: %s\n", msg.mtext);
    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q43.c -o HOL_Q43
// gcc HOL_Q44_b.c -o HOL_Q44_b
// ./HOL_Q44_b                -> queue empty: returns at once with ENOMSG
// ./HOL_Q43
// ./HOL_Q44_b                -> message received
