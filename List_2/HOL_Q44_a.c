// Receive from the message queue with flag 0 (blocks until a message arrives)
#include <stdio.h>
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

    printf("Waiting (blocking) for a message...\n");
    if (msgrcv(msgid, &msg, sizeof(msg.mtext), 1, 0) == -1) {
        perror("msgrcv");
        return 1;
    }

    printf("Received: %s\n", msg.mtext);
    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q43.c -o HOL_Q43
// gcc HOL_Q44_a.c -o HOL_Q44_a
// Case 1: ./HOL_Q43 then ./HOL_Q44_a         -> message received immediately
// Case 2 (flag 0 blocks):
//   Terminal 1: ./HOL_Q44_a                  -> waits, queue is empty
//   Terminal 2: ./HOL_Q43                    -> terminal 1 now receives it
// ipcs -q                                    -> message count drops after receiving
