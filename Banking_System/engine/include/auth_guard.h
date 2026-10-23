#ifndef AUTH_GUARD_H
#define AUTH_GUARD_H

/*
 * Hand-rolled brute-force lockout guard.
 *
 * After MAX_FAILED_ATTEMPTS consecutive failed logins for a given
 * (kind, id), further login attempts against that id are refused for
 * LOCKOUT_SECONDS *without even checking the password* -- this is what
 * actually stops a fast automated password-guessing loop, since it removes
 * the attacker's ability to test guesses at all during the lockout window,
 * rather than just slowing down the check.
 *
 * This is separate, transient, in-memory state (not persisted to
 * accounts.dat/staff.dat/admins.dat) protected by one mutex -- a lockout
 * is a property of "how the server has been treated recently", not of the
 * account/staff/admin record itself.
 */

typedef enum {
    AUTH_USER = 0,
    AUTH_EMPLOYEE = 1,
    AUTH_MANAGER = 2,
    AUTH_ADMIN = 3,
    AUTH_STAFF = AUTH_EMPLOYEE
} AuthKind;

void auth_guard_init(void);

/* Returns seconds remaining before another attempt is allowed, or 0 if the
 * caller may attempt a login right now. */
int auth_guard_seconds_locked(AuthKind kind, int id);

void auth_guard_record_failure(AuthKind kind, int id);
void auth_guard_record_success(AuthKind kind, int id);

#endif
