// Concurrent TCP server using pthread_create() - each client is handled by a thread
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>

#define PORT 8082

void *handle_client(void *arg) {
    int client_fd = *(int *)arg;
    free(arg);

    char buf[256] = {0};
    int n = read(client_fd, buf, sizeof(buf) - 1);
    printf("[thread %lu] received: %s\n", pthread_self(), buf);

    char *reply = "Hello from the concurrent server (pthread)!";
    write(client_fd, reply, strlen(reply) + 1);

    close(client_fd);
    return NULL;
}

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr *)&addr, sizeof(addr));
    listen(server_fd, 5);
    printf("Concurrent (pthread) server listening on port %d...\n", PORT);

    while (1) {
        int *client_fd = malloc(sizeof(int));
        *client_fd = accept(server_fd, NULL, NULL);
        if (*client_fd == -1) {
            free(client_fd);
            continue;
        }

        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, client_fd);
        pthread_detach(tid);
    }

    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q52_b.c -o HOL_Q52_b -lpthread
// Terminal 1: ./HOL_Q52_b
// Terminal 2 (several clients at the same time):
//   for i in 1 2 3; do echo "client $i" | nc 127.0.0.1 8082 & done; wait
// Terminal 1 shows a different thread id for every client.
// ps -T -p $(pgrep HOL_Q52_b)          -> threads of the server process
// Stop the server with Ctrl+C.
