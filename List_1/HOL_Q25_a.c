// Execute an existing executable program
#include <stdio.h>
#include <unistd.h>

int main() {
    printf("Executing /bin/ls using execl...\n");
    execl("/bin/ls", "ls", "-l", NULL);
    perror("execl failed"); // only reached if exec fails
    return 1;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q25_a.c -o exec_demo_a
// ./exec_demo_a
