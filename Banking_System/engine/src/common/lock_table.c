#include "lock_table.h"
#include "common.h"

#include <pthread.h>
#include <stdlib.h>

#define LOCK_BUCKETS 257

typedef struct LockEntry {
    LockRecordType type;
    int record_id;
    unsigned int references;
    RWLock lock;
    struct LockEntry *next;
} LockEntry;

typedef struct {
    pthread_mutex_t mutex;
    LockEntry *head;
} LockBucket;

static LockBucket g_buckets[LOCK_BUCKETS];
static pthread_once_t g_registry_once = PTHREAD_ONCE_INIT;
static pthread_mutex_t g_creation_mutex = PTHREAD_MUTEX_INITIALIZER;

static void registry_init_once(void) {
    for (int i = 0; i < LOCK_BUCKETS; i++) {
        pthread_mutex_init(&g_buckets[i].mutex, NULL);
        g_buckets[i].head = NULL;
    }
}

static int valid_record(LockRecordType type, int record_id) {
    if (record_id <= 0) return 0;
    if (type == LOCK_RECORD_ACCOUNT) return record_id <= MAX_ACCOUNTS;
    if (type == LOCK_RECORD_ADMIN) return record_id <= MAX_ADMINS;
    if (type == LOCK_RECORD_STAFF) return record_id <= 2 * MAX_STAFF;
    if (type == LOCK_RECORD_LOAN) return record_id <= MAX_LOANS;
    return 0;
}

static unsigned int bucket_index(LockRecordType type, int record_id) {
    return ((unsigned int)record_id * 31u + (unsigned int)type) % LOCK_BUCKETS;
}

void lock_table_init(void) { pthread_once(&g_registry_once, registry_init_once); }

int lock_table_acquire(LockRecordType type, int record_id, RecordLockHandle *out) {
    if (!out || !valid_record(type, record_id)) return -1;
    lock_table_init();
    LockBucket *bucket = &g_buckets[bucket_index(type, record_id)];
    pthread_mutex_lock(&bucket->mutex);
    LockEntry *entry = bucket->head;
    while (entry && (entry->type != type || entry->record_id != record_id)) entry = entry->next;
    if (!entry) {
        entry = calloc(1, sizeof(*entry));
        if (!entry) { pthread_mutex_unlock(&bucket->mutex); return -1; }
        entry->type = type;
        entry->record_id = record_id;
        rwlock_init(&entry->lock);
        entry->next = bucket->head;
        bucket->head = entry;
    }
    entry->references++;
    out->lock = &entry->lock;
    out->entry = entry;
    pthread_mutex_unlock(&bucket->mutex);
    return 0;
}

void lock_table_release(RecordLockHandle *handle) {
    if (!handle || !handle->entry) return;
    LockEntry *entry = handle->entry;
    LockBucket *bucket = &g_buckets[bucket_index(entry->type, entry->record_id)];
    LockEntry *retired = NULL;
    pthread_mutex_lock(&bucket->mutex);
    if (entry->references > 0 && --entry->references == 0) {
        LockEntry **link = &bucket->head;
        while (*link && *link != entry) link = &(*link)->next;
        if (*link == entry) { *link = entry->next; retired = entry; }
    }
    pthread_mutex_unlock(&bucket->mutex);
    if (retired) { rwlock_destroy(&retired->lock); free(retired); }
    handle->lock = NULL;
    handle->entry = NULL;
}

void lock_table_creation_lock(void) { pthread_mutex_lock(&g_creation_mutex); }
void lock_table_creation_unlock(void) { pthread_mutex_unlock(&g_creation_mutex); }
