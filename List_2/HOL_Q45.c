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

    struct msqid_ds info;
    if (msgctl(msgid, IPC_STAT, &info) == -1) {
        perror("msgctl IPC_STAT");
        return 1;
    }

    printf("Old permission: %o\n", info.msg_perm.mode & 0777);

    info.msg_perm.mode = (info.msg_perm.mode & ~0777) | 0600;

    if (msgctl(msgid, IPC_SET, &info) == -1) {
        perror("msgctl IPC_SET");
        return 1;
    }

    msgctl(msgid, IPC_STAT, &info);
    printf("New permission: %o\n", info.msg_perm.mode & 0777);

    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q41.c -o HOL_Q41 && ./HOL_Q41
// ipcs -q                    -> perms column shows 666
// gcc HOL_Q45.c -o HOL_Q45
// ./HOL_Q45
// ipcs -q                    -> perms column now shows 600
