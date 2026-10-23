#include "rwlock.h"

void rwlock_init(RWLock *lock) {
    pthread_mutex_init(&lock->mutex, NULL);
    pthread_cond_init(&lock->readers_ok, NULL);
    pthread_cond_init(&lock->writer_ok, NULL);
    lock->active_readers = 0;
    lock->waiting_writers = 0;
    lock->active_writer = 0;
}

void rwlock_destroy(RWLock *lock) {
    pthread_mutex_destroy(&lock->mutex);
    pthread_cond_destroy(&lock->readers_ok);
    pthread_cond_destroy(&lock->writer_ok);
}

void rwlock_acquire_read(RWLock *lock) {
    pthread_mutex_lock(&lock->mutex);
    /* Block readers while a writer is active OR waiting (writer priority). */
    while (lock->active_writer || lock->waiting_writers > 0) {
        pthread_cond_wait(&lock->readers_ok, &lock->mutex);
    }
    lock->active_readers++;
    pthread_mutex_unlock(&lock->mutex);
}

void rwlock_release_read(RWLock *lock) {
    pthread_mutex_lock(&lock->mutex);
    lock->active_readers--;
    if (lock->active_readers == 0) {
        pthread_cond_signal(&lock->writer_ok);
    }
    pthread_mutex_unlock(&lock->mutex);
}

void rwlock_acquire_write(RWLock *lock) {
    pthread_mutex_lock(&lock->mutex);
    lock->waiting_writers++;
    while (lock->active_writer || lock->active_readers > 0) {
        pthread_cond_wait(&lock->writer_ok, &lock->mutex);
    }
    lock->waiting_writers--;
    lock->active_writer = 1;
    pthread_mutex_unlock(&lock->mutex);
}

void rwlock_release_write(RWLock *lock) {
    pthread_mutex_lock(&lock->mutex);
    lock->active_writer = 0;
    /* Wake every waiting reader and one waiting writer; whichever the
     * scheduler runs first will re-check the guard condition above. */
    pthread_cond_broadcast(&lock->readers_ok);
    pthread_cond_signal(&lock->writer_ok);
    pthread_mutex_unlock(&lock->mutex);
}
