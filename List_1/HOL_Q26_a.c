// ls -Rl using execl
#include <stdio.h>
#include <unistd.h>

int main() {
    execl("/bin/ls", "ls", "-Rl", NULL);
    perror("execl failed");
    return 1;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q26_a.c -o execl_demo
// ./execl_demo
