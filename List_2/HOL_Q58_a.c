#include <stdio.h>
#include <pthread.h>

void *thread_func(void *arg) {
    long id = (long)arg;
    printf("Thread %ld running (tid=%lu)\n", id, pthread_self());
    return NULL;
}

int main() {
    pthread_t threads[3];

    for (long i = 0; i < 3; i++) {
        if (pthread_create(&threads[i], NULL, thread_func, (void *)i) != 0) {
            perror("pthread_create");
            return 1;
        }
    }

    for (int i = 0; i < 3; i++) {
        pthread_join(threads[i], NULL);
    }

    return 0;
}

// Commands to run the program (for viva demonstration):
// gcc HOL_Q58_a.c -o HOL_Q58_a -lpthread
// ./HOL_Q58_a                -> three threads each print their own id
