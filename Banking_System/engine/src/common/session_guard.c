#include "session_guard.h"
#include "common.h"

#include <pthread.h>

static int g_user_owner1[MAX_ACCOUNTS + 1];
static int g_user_owner2[MAX_ACCOUNTS + 1];
static int g_staff[2 * MAX_STAFF + 1];
static int g_admin[MAX_ADMINS + 1];
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;

void session_guard_init(void) {
    /* Static storage is already zero-initialized. */
}

static int *slot_for(GuardKind kind, int id, int owner_slot) {
    switch (kind) {
        case GUARD_USER:
            if (id < 0 || id > MAX_ACCOUNTS) return NULL;
            return (owner_slot == 2) ? &g_user_owner2[id] : &g_user_owner1[id];
        case GUARD_STAFF:
            if (id < 0 || id > 2 * MAX_STAFF) return NULL;
            return &g_staff[id];
        case GUARD_ADMIN:
            if (id < 0 || id > MAX_ADMINS) return NULL;
            return &g_admin[id];
        default:
            return NULL;
    }
}

int session_guard_acquire(GuardKind kind, int id, int owner_slot) {
    int *slot = slot_for(kind, id, owner_slot);
    if (!slot) return 0;

    pthread_mutex_lock(&g_mutex);
    int acquired = 0;
    if (!*slot) {
        *slot = 1;
        acquired = 1;
    }
    pthread_mutex_unlock(&g_mutex);
    return acquired;
}

void session_guard_release(GuardKind kind, int id, int owner_slot) {
    int *slot = slot_for(kind, id, owner_slot);
    if (!slot) return;

    pthread_mutex_lock(&g_mutex);
    *slot = 0;
    pthread_mutex_unlock(&g_mutex);
}
