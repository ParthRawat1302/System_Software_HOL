#include <stdio.h>
#include <sys/ipc.h>
#include <sys/msg.h>

int main() {
    key_t key = ftok("HOL_Q41.c", 65); // any existing file + a project id; Q42-Q46 use this same key

    int msgid = msgget(key, 0666 | IPC_CREAT);
    if (msgid == -1) {
        perror("msgget");
        return 1;
    }

    printf("Key: %d (0x%x)\n", key, key); // hex form matches the ipcs -q "key" column
    printf("Message queue id: %d\n", msgid);

    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q41.c -o HOL_Q41
// ./HOL_Q41
// ipcs -q                    -> the key (hex) and msqid printed above are listed
// Note: HOL_Q42.c to HOL_Q46.c use this same key, so they all work on this queue.
