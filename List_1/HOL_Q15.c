#include <stdio.h>

extern char **environ;

int main() {
    for (int i = 0; environ[i] != NULL; i++) {
        printf("%s\n", environ[i]);
    }
    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q15.c -o environ_vars
// ./environ_vars
// ./environ_vars | wc -l
// export MY_VAR=hello_viva
// ./environ_vars | grep MY_VAR
// (compare against the shell's own: env   or   printenv)
