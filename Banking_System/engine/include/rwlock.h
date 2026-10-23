#ifndef RWLOCK_H
#define RWLOCK_H

#include <pthread.h>

/*
 * Hand-rolled readers-writer lock.
 *
 * We deliberately do NOT use pthread_rwlock_t (that IS the premade
 * reader/writer lock) and we do NOT use fcntl()/flock() record locking
 * (that IS the premade file-locking service). Instead this builds the
 * classic writer-priority readers/writers algorithm ourselves out of the
 * lower-level primitives pthread_mutex_t + pthread_cond_t.
 *
 * Writer priority: once a writer is waiting, new readers block behind it,
 * so a busy joint account can't starve a pending deposit/withdraw.
 */
typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t  readers_ok;
    pthread_cond_t  writer_ok;
    int active_readers;
    int waiting_writers;
    int active_writer;
} RWLock;

void rwlock_init(RWLock *lock);
void rwlock_destroy(RWLock *lock);

void rwlock_acquire_read(RWLock *lock);
void rwlock_release_read(RWLock *lock);

void rwlock_acquire_write(RWLock *lock);
void rwlock_release_write(RWLock *lock);

#endif
