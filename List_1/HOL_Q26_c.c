// ls -Rl using execle (caller supplies the environment explicitly)
#include <stdio.h>
#include <unistd.h>

extern char **environ;

int main() {
    execle("/bin/ls", "ls", "-Rl", NULL, environ);
    perror("execle failed");
    return 1;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q26_c.c -o execle_demo
// ./execle_demo
