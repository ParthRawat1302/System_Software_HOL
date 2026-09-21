// ls -Rl using execvp (array of arguments + PATH search)
#include <stdio.h>
#include <unistd.h>

int main() {
    char *args[] = {"ls", "-Rl", NULL};
    execvp("ls", args);
    perror("execvp failed");
    return 1;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q26_e.c -o execvp_demo
// ./execvp_demo
