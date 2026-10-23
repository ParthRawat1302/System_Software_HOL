#ifndef LOCK_TABLE_H
#define LOCK_TABLE_H

#include "rwlock.h"

/* Locks are allocated on demand and indexed by (record class, record id).
 * A caller owns one reference from lock_table_acquire() until
 * lock_table_release(). That reference remains held while it waits for or
 * owns the RWLock, so an idle entry is never destroyed under another thread. */
typedef enum {
    LOCK_RECORD_ACCOUNT,
    LOCK_RECORD_ADMIN,
    LOCK_RECORD_STAFF,
    LOCK_RECORD_LOAN
} LockRecordType;

typedef struct {
    RWLock *lock;
    void *entry;                 /* private registry entry; do not inspect */
} RecordLockHandle;

void lock_table_init(void);
int lock_table_acquire(LockRecordType type, int record_id, RecordLockHandle *out);
void lock_table_release(RecordLockHandle *handle);

/* Serialises account *creation* specifically.
 *
 * Creating an account is find-a-free-slot-then-write, and the per-record
 * RWLock above cannot protect that: you don't know which record to lock
 * until the search has already finished. Without this mutex, two
 * concurrent creations can both pick the same free slot and both write
 * to it -- the second silently destroying the first while both callers
 * are told their account was created (measured: 40 creations produced
 * only 28 accounts; see docs/VIVA_GUIDE.md).
 *
 * Lock order is creation-mutex -> account RWLock, and nothing else ever
 * takes the creation mutex, so no cycle (and therefore no deadlock) is
 * possible. */
void lock_table_creation_lock(void);
void lock_table_creation_unlock(void);

#endif
