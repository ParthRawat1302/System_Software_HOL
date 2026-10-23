#include "auth_guard.h"
#include "common.h"

#include <pthread.h>
#include <time.h>
#include <stddef.h>

#define MAX_FAILED_ATTEMPTS 5
#define LOCKOUT_SECONDS 30

typedef struct {
    int fail_count;
    time_t locked_until;
} GuardEntry;

static GuardEntry g_user_guard[MAX_ACCOUNTS + 1];
static GuardEntry g_staff_guard[2 * MAX_STAFF + 1];
static GuardEntry g_admin_guard[MAX_ADMINS + 1];
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;

static GuardEntry *entry_for(AuthKind kind, int id) {
    switch (kind) {
        case AUTH_USER:  return (id >= 0 && id <= MAX_ACCOUNTS) ? &g_user_guard[id] : NULL;
        case AUTH_EMPLOYEE:
        case AUTH_MANAGER:
            return (id >= 0 && id <= 2 * MAX_STAFF) ? &g_staff_guard[id] : NULL;
        case AUTH_ADMIN: return (id >= 0 && id <= MAX_ADMINS) ? &g_admin_guard[id] : NULL;
        default: return NULL;
    }
}

void auth_guard_init(void) {
    /* Static storage is already zero-initialized. */
}

int auth_guard_seconds_locked(AuthKind kind, int id) {
    GuardEntry *e = entry_for(kind, id);
    if (!e) return 0;

    pthread_mutex_lock(&g_mutex);
    time_t now = time(NULL);
    int remaining = (e->locked_until > now) ? (int)(e->locked_until - now) : 0;
    pthread_mutex_unlock(&g_mutex);
    return remaining;
}

void auth_guard_record_failure(AuthKind kind, int id) {
    GuardEntry *e = entry_for(kind, id);
    if (!e) return;

    pthread_mutex_lock(&g_mutex);
    e->fail_count++;
    if (e->fail_count >= MAX_FAILED_ATTEMPTS) {
        e->locked_until = time(NULL) + LOCKOUT_SECONDS;
        e->fail_count = 0;
    }
    pthread_mutex_unlock(&g_mutex);
}

void auth_guard_record_success(AuthKind kind, int id) {
    GuardEntry *e = entry_for(kind, id);
    if (!e) return;

    pthread_mutex_lock(&g_mutex);
    e->fail_count = 0;
    e->locked_until = 0;
    pthread_mutex_unlock(&g_mutex);
}
