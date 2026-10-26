#include <stdio.h>
#include <sys/resource.h>

void print_limit(const char *name, int resource) {
    struct rlimit rl;
    getrlimit(resource, &rl);

    printf("%-15s soft=", name);
    if (rl.rlim_cur == RLIM_INFINITY) printf("infinity"); else printf("%ld", (long)rl.rlim_cur);
    printf("  hard=");
    if (rl.rlim_max == RLIM_INFINITY) printf("infinity"); else printf("%ld", (long)rl.rlim_max);
    printf("\n");
}

int main() {
    print_limit("RLIMIT_CPU", RLIMIT_CPU);
    print_limit("RLIMIT_FSIZE", RLIMIT_FSIZE);
    print_limit("RLIMIT_DATA", RLIMIT_DATA);
    print_limit("RLIMIT_STACK", RLIMIT_STACK);
    print_limit("RLIMIT_NOFILE", RLIMIT_NOFILE);
    print_limit("RLIMIT_NPROC", RLIMIT_NPROC);
    print_limit("RLIMIT_AS", RLIMIT_AS);

    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q54.c -o HOL_Q54
// ./HOL_Q54
// ulimit -a                  -> compare the soft limits
// ulimit -Ha                 -> compare the hard limits
