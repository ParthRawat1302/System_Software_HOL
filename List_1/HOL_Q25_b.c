// Pass an argument to an executable program (build HOL_Q25_c.c to ./greet first)
#include <stdio.h>
#include <unistd.h>

int main() {
    printf("Executing ./greet with argument 'World'\n");
    execl("./greet", "greet", "World", NULL);
    perror("execl failed");
    return 1;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q25_c.c -o greet
// gcc HOL_Q25_b.c -o exec_demo_b
// ./exec_demo_b      -> execs ./greet, which prints "Hello, World!"
