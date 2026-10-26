#include <stdio.h>
#include <pthread.h>

void *thread_func(void *arg) {
    printf("Created thread id: %lu\n", pthread_self());
    return NULL;
}

int main() {
    pthread_t threads[3];

    for (int i = 0; i < 3; i++) {
        pthread_create(&threads[i], NULL, thread_func, NULL);
    }

    printf("Main thread id: %lu\n", pthread_self());

    for (int i = 0; i < 3; i++) {
        printf("Stored thread id [%d]: %lu\n", i, threads[i]);
        pthread_join(threads[i], NULL);
    }

    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q58_b.c -o HOL_Q58_b -lpthread
// ./HOL_Q58_b                -> ids printed inside the threads match the ids
//                               stored by pthread_create() in the main thread
