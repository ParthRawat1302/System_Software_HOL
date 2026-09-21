// ls -Rl using execv (arguments passed as an array instead of a variadic list)
#include <stdio.h>
#include <unistd.h>

int main() {
    char *args[] = {"ls", "-Rl", NULL};
    execv("/bin/ls", args);
    perror("execv failed");
    return 1;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q26_d.c -o execv_demo
// ./execv_demo
