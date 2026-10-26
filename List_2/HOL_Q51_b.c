// TCP client. Usage: ./HOL_Q51_b <server-ip>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <server-ip>\n", argv[0]);
        return 1;
    }

    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, argv[1], &addr.sin_addr) <= 0) {
        perror("inet_pton");
        return 1;
    }

    if (connect(sock_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("connect");
        return 1;
    }

    char *msg = "Hello from the client!";
    write(sock_fd, msg, strlen(msg) + 1);

    char buf[256] = {0};
    int n = read(sock_fd, buf, sizeof(buf) - 1);
    printf("Received from server: %s\n", buf);

    close(sock_fd);
    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q51_b.c -o HOL_Q51_b
// Start the server first on the other machine: ./HOL_Q51_a
// ./HOL_Q51_b <ip-of-server>        (same machine: ./HOL_Q51_b 127.0.0.1)
// Expected: Received from server: Hello from the server!
