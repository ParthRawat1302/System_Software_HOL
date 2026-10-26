#include <stdio.h>
#include <unistd.h>
#include <stdint.h>

static inline uint64_t rdtsc(void) {
    unsigned int lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

int main() {
    uint64_t start = rdtsc();

    for (int i = 0; i < 100; i++) {
        getppid();
    }

    uint64_t end = rdtsc();

    printf("Cycles for 100 getppid() calls: %llu\n", (unsigned long long)(end - start));
    printf("Average cycles per call: %llu\n", (unsigned long long)((end - start) / 100));

    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q56.c -o HOL_Q56
// ./HOL_Q56                  -> total and average CPU cycles (x86/x86-64 only: rdtsc)
// Run it a few times; the first run is usually slower (cold caches).
