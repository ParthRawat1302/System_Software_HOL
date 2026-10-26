#include <stdio.h>
#include <string.h>
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
    msg.mtype = 1;
    strcpy(msg.mtext, "Hello via message queue!");

    if (msgsnd(msgid, &msg, sizeof(msg.mtext), 0) == -1) {
        perror("msgsnd");
        return 1;
    }

    printf("Message sent: %s\n", msg.mtext);
    printf("Check with: ipcs -q\n");

    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q43.c -o HOL_Q43
// ./HOL_Q43
// ./HOL_Q43                  (run a few times)
// ipcs -q                    -> "messages" column increases with every run
