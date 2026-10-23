#ifndef SESSION_H
#define SESSION_H

#include "common.h"

typedef enum { SESSION_NONE = 0, SESSION_USER = 1, SESSION_ADMIN = 2, SESSION_STAFF = 3 } SessionRole;

/* One connection = one thread = one session, held on that thread's stack.
 * No shared session table/global mutex is needed for the account data
 * itself (a socket fd is only ever touched by the single thread that
 * accepted it) -- session_guard.h is a separate, deliberate exception:
 * it tracks *identity* uniqueness ("one session per user") across all
 * connections, which is inherently shared state. */
typedef struct {
    SessionRole role;
    int account_id;      /* valid when role == SESSION_USER */
    int as_owner2;         /* 1 if the joint account's *second* holder logged in */
    int admin_id;          /* valid when role == SESSION_ADMIN */
    int staff_id;           /* valid when role == SESSION_STAFF */
    StaffRole staff_role;    /* STAFF_EMPLOYEE or STAFF_MANAGER, valid when role == SESSION_STAFF */
    int guard_acquired;       /* whether this connection currently holds a session_guard slot */
} ClientSession;

#endif
