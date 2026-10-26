#include <stdio.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <time.h>

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

    printf("Access permission : %o\n", info.msg_perm.mode & 0777);
    printf("UID / GID          : %d / %d\n", info.msg_perm.uid, info.msg_perm.gid);
    printf("Last msgsnd time   : %s", info.msg_stime ? ctime(&info.msg_stime) : "never\n");
    printf("Last msgrcv time   : %s", info.msg_rtime ? ctime(&info.msg_rtime) : "never\n");
    printf("Last change time   : %s", ctime(&info.msg_ctime));
    printf("Current queue size : %lu bytes\n", (unsigned long)info.msg_cbytes);
    printf("Messages in queue  : %lu\n", (unsigned long)info.msg_qnum);
    printf("Max bytes allowed  : %lu\n", (unsigned long)info.msg_qbytes);
    printf("Last msgsnd pid    : %d\n", info.msg_lspid);
    printf("Last msgrcv pid    : %d\n", info.msg_lrpid);

    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q41.c -o HOL_Q41 && ./HOL_Q41   (create the queue)
// gcc HOL_Q43.c -o HOL_Q43 && ./HOL_Q43   (send a message so the counters change)
// gcc HOL_Q42.c -o HOL_Q42
// ./HOL_Q42
// ipcs -q -i <msqid>         -> compare with the kernel's own view of the queue
