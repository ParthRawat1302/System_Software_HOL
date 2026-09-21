#include <stdio.h>
#include <unistd.h>
#include <sys/select.h>

int main() {
    fd_set readfds;
    struct timeval tv;
    int ret;

    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);

    tv.tv_sec = 10;
    tv.tv_usec = 0;

    printf("Waiting for input on STDIN for 10 seconds...\n");
    ret = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &tv);

    if (ret == -1) {
        perror("select error");
        return 1;
    } else if (ret == 0) {
        printf("Timeout! No data received within 10 seconds.\n");
    } else {
        char buf[256];
        int n = read(STDIN_FILENO, buf, sizeof(buf) - 1);
        buf[n] = '\0';
        printf("Data received: %s", buf);
    }

    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q13.c -o select_stdin
// ./select_stdin
// (type something within 10 seconds and press Enter - it is echoed back)
//
// ./select_stdin
// (do nothing for 10 seconds - see the "Timeout!" message)
//
// man 2 select     -> to show the select() man page mentioned in the question
