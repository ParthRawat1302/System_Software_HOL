// Concurrent TCP server using fork() - each client is handled by a child process
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/wait.h>

#define PORT 8081

int main() {
    signal(SIGCHLD, SIG_IGN); // auto-reap finished children, no zombies

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr *)&addr, sizeof(addr));
    listen(server_fd, 5);
    printf("Concurrent (fork) server listening on port %d...\n", PORT);

    while (1) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd == -1) continue;

        pid_t pid = fork();
        if (pid == 0) {
            close(server_fd);

            char buf[256] = {0};
            int n = read(client_fd, buf, sizeof(buf) - 1);
            printf("[child %d] received: %s\n", getpid(), buf);

            char *reply = "Hello from the concurrent server (fork)!";
            write(client_fd, reply, strlen(reply) + 1);

            close(client_fd);
            _exit(0);
        }

        close(client_fd); // parent doesn't need this copy
    }

    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q52_a.c -o HOL_Q52_a
// Terminal 1: ./HOL_Q52_a
// Terminal 2 (several clients at the same time):
//   for i in 1 2 3; do echo "client $i" | nc 127.0.0.1 8081 & done; wait
// Terminal 1 shows a different child pid for every client.
// Stop the server with Ctrl+C.
