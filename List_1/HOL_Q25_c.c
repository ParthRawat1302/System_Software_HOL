// Helper executable used by HOL_Q25_b.c - build separately: gcc HOL_Q25_c.c -o greet
#include <stdio.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <name>\n", argv[0]);
        return 1;
    }
    printf("Hello, %s!\n", argv[1]);
    return 0;
}

// Commands to run standalone (for viva demonstration):
// gcc HOL_Q25_c.c -o greet
// ./greet World       -> passing input from the command line, per part (b)
