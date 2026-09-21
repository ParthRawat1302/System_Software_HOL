// Online ticket reservation: create the initial ticket file
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

int main() {
    int fd = open("ticket.txt", O_CREAT | O_WRONLY | O_TRUNC, 0666);
    if (fd == -1) {
        perror("open");
        return 1;
    }

    int ticket = 100;
    char buf[16];
    int len = sprintf(buf, "%d", ticket);
    write(fd, buf, len);
    close(fd);

    printf("Initial ticket number %d stored in ticket.txt\n", ticket);
    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q17_a.c -o create_ticket
// ./create_ticket
// cat ticket.txt
