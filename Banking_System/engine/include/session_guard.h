#ifndef SESSION_GUARD_H
#define SESSION_GUARD_H

/*
 * Enforces "one session per user" (every role in the brief lists this).
 *
 * The identity that must be unique is the actual credential used to log
 * in, not just the account: on a joint account, owner1 and owner2 are two
 * different real people and must be allowed to be logged in at the same
 * time (that's the whole point of the joint-account concurrency story
 * elsewhere in this engine) -- so a USER session's key is
 * (account_id, owner_slot), not just account_id. Employee and Manager
 * guard slots occupy disjoint ranges, so both roles may use ID 1.
 *
 * Plain mutex-protected boolean tables -- this is presence/uniqueness
 * tracking, not a counting resource, so a mutex is the right primitive
 * here (the connection-limiting semaphore in server.c is the deliberate
 * counting-semaphore use elsewhere in this project).
 */

typedef enum { GUARD_USER = 0, GUARD_STAFF = 1, GUARD_ADMIN = 2 } GuardKind;

void session_guard_init(void);

/* Returns 1 if this identity was not already logged in (and marks it as
 * logged in now), or 0 if it was already active elsewhere. `owner_slot`
 * is 1 or 2 for GUARD_USER, ignored otherwise. */
int session_guard_acquire(GuardKind kind, int id, int owner_slot);

void session_guard_release(GuardKind kind, int id, int owner_slot);

#endif
