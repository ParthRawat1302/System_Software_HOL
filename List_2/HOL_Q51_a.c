// TCP server. Run on one machine, then run HOL_Q51_b.c with its IP address
// from the other machine. Listens on port 8080.
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind");
        return 1;
    }

    listen(server_fd, 5);
    printf("Server listening on port %d...\n", PORT);

    int client_fd = accept(server_fd, NULL, NULL);
    if (client_fd == -1) {
        perror("accept");
        return 1;
    }

    char buf[256] = {0};
    int n = read(client_fd, buf, sizeof(buf) - 1);
    printf("Received from client: %s\n", buf);

    char *reply = "Hello from the server!";
    write(client_fd, reply, strlen(reply) + 1);

    close(client_fd);
    close(server_fd);
    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q51_a.c -o HOL_Q51_a
// gcc HOL_Q51_b.c -o HOL_Q51_b
// Machine 1 (server): hostname -I   (note the IP address)
//                     ./HOL_Q51_a
// Machine 2 (client): ./HOL_Q51_b <ip-of-machine-1>
// Same machine test:  ./HOL_Q51_a  and in another terminal  ./HOL_Q51_b 127.0.0.1
// If it cannot connect, allow the port: sudo ufw allow 8080/tcp
