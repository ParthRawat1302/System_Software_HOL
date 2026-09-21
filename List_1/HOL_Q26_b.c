// ls -Rl using execlp (searches PATH, so no need for the full binary path)
#include <stdio.h>
#include <unistd.h>

int main() {
    execlp("ls", "ls", "-Rl", NULL);
    perror("execlp failed");
    return 1;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q26_b.c -o execlp_demo
// ./execlp_demo
