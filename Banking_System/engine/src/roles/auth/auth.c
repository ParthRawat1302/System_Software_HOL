#include "common.h"
#include "protocol.h"
#include "storage.h"
#include "lock_table.h"
#include "session.h"
#include "util.h"
#include "auth_guard.h"
#include "session_guard.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define ipc_send_log(unused, ...) ((void)(unused))
void handle_login(int fd, char *payload, ClientSession *sess, int msqid) {
    char *fields[3];
    if (count_char(payload, '|') != 2 || split_fields(payload, fields, 3) != 3) {
        send_frame(fd, OP_LOGIN, "ERR|malformed login, expected KIND|id|password");
        return;
    }
    /* The wire protocol documents uppercase kinds, but normalize here so
     * terminal/API clients using "manager" or "Employee" get the same
     * staff login path instead of a misleading unknown-kind error. */
    char normalized_kind[16];
    size_t kind_len = strlen(fields[0]);
    if (kind_len == 0 || kind_len >= sizeof(normalized_kind)) {
        send_frame(fd, OP_LOGIN, "ERR|unknown login kind");
        return;
    }
    for (size_t i = 0; i <= kind_len; i++) {
        normalized_kind[i] = (char)toupper((unsigned char)fields[0][i]);
    }
    const char *kind = normalized_kind;
    int id = atoi(fields[1]);
    const char *password = fields[2];

    AuthKind auth_kind;
    StaffRole requested_staff_role = STAFF_EMPTY;
    if (strcmp(kind, "ADMIN") == 0) auth_kind = AUTH_ADMIN;
    else if (strcmp(kind, "EMPLOYEE") == 0 || strcmp(kind, "STAFF") == 0) {
        auth_kind = AUTH_EMPLOYEE;
        requested_staff_role = STAFF_EMPLOYEE;
    } else if (strcmp(kind, "MANAGER") == 0) {
        auth_kind = AUTH_MANAGER;
        requested_staff_role = STAFF_MANAGER;
    }
    else if (strcmp(kind, "USER") == 0) auth_kind = AUTH_USER;
    else { send_frame(fd, OP_LOGIN, "ERR|unknown login kind"); return; }

    int guard_id = requested_staff_role == STAFF_EMPTY
                       ? id
                       : storage_staff_lock_id(requested_staff_role, id);
    int locked_secs = auth_guard_seconds_locked(auth_kind, guard_id);
    if (locked_secs > 0) {
        char msg[LOG_MSG_LEN];
        snprintf(msg, sizeof(msg), "ERR|too many failed attempts, try again in %ds", locked_secs);
        send_frame(fd, OP_LOGIN, msg);
        return;
    }

    /* A password containing '|' or ';' can never match a stored hash --
     * every password is barred from those characters at creation/change
     * time too -- so this is just another wrong-credentials case, and
     * still counts toward the lockout like any other failed attempt. */
    if (has_reserved_char(password)) {
        auth_guard_record_failure(auth_kind, id);
        send_frame(fd, OP_LOGIN, "ERR|invalid credentials");
        return;
    }

    unsigned long hash = simple_hash(password);
    char log[LOG_MSG_LEN];

    if (auth_kind == AUTH_ADMIN) {
        Admin adm;
        RecordLockHandle admin_lock;
        int admin_rc = lock_table_acquire(LOCK_RECORD_ADMIN, id, &admin_lock);
        if (admin_rc == 0) rwlock_acquire_read(admin_lock.lock);
        int read_rc = admin_rc == 0 ? storage_read_admin(id, &adm) : -1;
        if (admin_rc == 0) { rwlock_release_read(admin_lock.lock); lock_table_release(&admin_lock); }
        if (read_rc != 0 || adm.status != STATUS_ACTIVE || adm.pw_hash != hash) {
            auth_guard_record_failure(AUTH_ADMIN, id);
            snprintf(log, sizeof(log), "LOGIN_FAIL kind=ADMIN id=%d", id);
            ipc_send_log(msqid, log);
            send_frame(fd, OP_LOGIN, "ERR|invalid admin credentials");
            return;
        }
        if (!session_guard_acquire(GUARD_ADMIN, id, 0)) {
            send_frame(fd, OP_LOGIN, "ERR|already logged in from another session");
            return;
        }
        auth_guard_record_success(AUTH_ADMIN, id);
        sess->role = SESSION_ADMIN;
        sess->admin_id = id;
        sess->guard_acquired = 1;
        snprintf(log, sizeof(log), "LOGIN_OK kind=ADMIN id=%d", id);
        ipc_send_log(msqid, log);
        send_frame(fd, OP_LOGIN, "OK|welcome admin");
        return;
    }

    if (requested_staff_role != STAFF_EMPTY) {
        Staff st;
        RecordLockHandle staff_lock;
        int staff_rc = lock_table_acquire(LOCK_RECORD_STAFF, guard_id, &staff_lock);
        if (staff_rc == 0) rwlock_acquire_read(staff_lock.lock);
        int read_rc = staff_rc == 0 ? storage_read_staff(requested_staff_role, id, &st) : -1;
        if (staff_rc == 0) { rwlock_release_read(staff_lock.lock); lock_table_release(&staff_lock); }
        if (read_rc != 0 || st.status != STATUS_ACTIVE || st.pw_hash != hash) {
            auth_guard_record_failure(auth_kind, guard_id);
            snprintf(log, sizeof(log), "LOGIN_FAIL kind=%s id=%d",
                     requested_staff_role == STAFF_MANAGER ? "MANAGER" : "EMPLOYEE", id);
            ipc_send_log(msqid, log);
            send_frame(fd, OP_LOGIN, "ERR|invalid staff credentials");
            return;
        }
        if (!session_guard_acquire(GUARD_STAFF, guard_id, 0)) {
            send_frame(fd, OP_LOGIN, "ERR|already logged in from another session");
            return;
        }
        auth_guard_record_success(auth_kind, guard_id);
        sess->role = SESSION_STAFF;
        sess->staff_id = id;
        sess->staff_role = requested_staff_role;
        sess->guard_acquired = 1;
        snprintf(log, sizeof(log), "LOGIN_OK kind=%s id=%d",
                 requested_staff_role == STAFF_MANAGER ? "MANAGER" : "EMPLOYEE", id);
        ipc_send_log(msqid, log);
        send_frame(fd, OP_LOGIN, requested_staff_role == STAFF_MANAGER
                                   ? "OK|welcome manager" : "OK|welcome employee");
        return;
    }

    /* AUTH_USER */
    RecordLockHandle account_lock;
    if (lock_table_acquire(LOCK_RECORD_ACCOUNT, id, &account_lock) != 0) {
        send_frame(fd, OP_LOGIN, "ERR|invalid account id"); return;
    }

    rwlock_acquire_read(account_lock.lock);
    Account acc;
    int rc = storage_read_account(id, &acc);
    rwlock_release_read(account_lock.lock);
    lock_table_release(&account_lock);

    if (rc != 0 || acc.status != STATUS_ACTIVE) {
        auth_guard_record_failure(AUTH_USER, id);
        snprintf(log, sizeof(log), "LOGIN_FAIL kind=USER id=%d", id);
        ipc_send_log(msqid, log);
        send_frame(fd, OP_LOGIN, "ERR|invalid account credentials");
        return;
    }

    int owner_slot = 0;
    if (acc.owner1_pw_hash == hash) owner_slot = 1;
    else if (acc.role == ROLE_JOINT && acc.owner2_pw_hash == hash) owner_slot = 2;

    if (owner_slot == 0) {
        auth_guard_record_failure(AUTH_USER, id);
        snprintf(log, sizeof(log), "LOGIN_FAIL kind=USER id=%d", id);
        ipc_send_log(msqid, log);
        send_frame(fd, OP_LOGIN, "ERR|invalid account credentials");
        return;
    }

    if (!session_guard_acquire(GUARD_USER, id, owner_slot)) {
        send_frame(fd, OP_LOGIN, "ERR|already logged in from another session");
        return;
    }
    auth_guard_record_success(AUTH_USER, id);
    sess->role = SESSION_USER;
    sess->account_id = id;
    sess->as_owner2 = (owner_slot == 2);
    sess->guard_acquired = 1;
    snprintf(log, sizeof(log), "LOGIN_OK kind=USER id=%d owner=%d", id, owner_slot);
    ipc_send_log(msqid, log);
    send_frame(fd, OP_LOGIN, "OK|welcome");
}

/* ---------- customer operations ---------- */


void handle_change_pw(int fd, char *payload, ClientSession *sess, int msqid) {
    if (sess->role == SESSION_NONE) { send_frame(fd, OP_CHANGE_PW, "ERR|not logged in"); return; }
    char *fields[2];
    if (count_char(payload, '|') != 1 || split_fields(payload, fields, 2) != 2) {
        send_frame(fd, OP_CHANGE_PW, "ERR|malformed request, expected old|new");
        return;
    }
    if (has_reserved_char(fields[0]) || has_reserved_char(fields[1])) {
        send_frame(fd, OP_CHANGE_PW, "ERR|passwords must not contain '|' or ';'");
        return;
    }
    unsigned long old_hash = simple_hash(fields[0]);
    unsigned long new_hash = simple_hash(fields[1]);

    if (sess->role == SESSION_USER) {
        RecordLockHandle account_lock;
        if (lock_table_acquire(LOCK_RECORD_ACCOUNT, sess->account_id, &account_lock) != 0) {
            send_frame(fd, OP_CHANGE_PW, "ERR|account unavailable"); return;
        }
        rwlock_acquire_write(account_lock.lock);
        Account acc;
        if (storage_read_account(sess->account_id, &acc) != 0 || acc.status != STATUS_ACTIVE) {
            rwlock_release_write(account_lock.lock);
            lock_table_release(&account_lock);
            send_frame(fd, OP_CHANGE_PW, "ERR|account unavailable");
            return;
        }
        unsigned long *slot = sess->as_owner2 ? &acc.owner2_pw_hash : &acc.owner1_pw_hash;
        if (*slot != old_hash) {
            rwlock_release_write(account_lock.lock);
            lock_table_release(&account_lock);
            send_frame(fd, OP_CHANGE_PW, "ERR|old password incorrect");
            return;
        }
        *slot = new_hash;
        int rc = storage_write_account(&acc);
        rwlock_release_write(account_lock.lock);
        lock_table_release(&account_lock);
        if (rc == 0) {
            char log[LOG_MSG_LEN];
            snprintf(log, sizeof(log), "PASSWORD_CHANGE kind=USER account=%d owner=%d", sess->account_id, sess->as_owner2 ? 2 : 1);
            ipc_send_log(msqid, log);
        }
        send_frame(fd, OP_CHANGE_PW, rc == 0 ? "OK|password changed" : "ERR|write failed");
        return;
    }

    if (sess->role == SESSION_STAFF) {
        int staff_lock_id = storage_staff_lock_id(sess->staff_role, sess->staff_id);
        RecordLockHandle staff_lock;
        if (lock_table_acquire(LOCK_RECORD_STAFF, staff_lock_id, &staff_lock) != 0) {
            send_frame(fd, OP_CHANGE_PW, "ERR|account unavailable"); return;
        }
        rwlock_acquire_write(staff_lock.lock);
        Staff st;
        if (storage_read_staff(sess->staff_role, sess->staff_id, &st) != 0 || st.status != STATUS_ACTIVE) {
            rwlock_release_write(staff_lock.lock);
            lock_table_release(&staff_lock);
            send_frame(fd, OP_CHANGE_PW, "ERR|account unavailable");
            return;
        }
        if (st.pw_hash != old_hash) {
            rwlock_release_write(staff_lock.lock);
            lock_table_release(&staff_lock);
            send_frame(fd, OP_CHANGE_PW, "ERR|old password incorrect"); return;
        }
        st.pw_hash = new_hash;
        int rc = storage_update_staff(sess->staff_role, &st);
        rwlock_release_write(staff_lock.lock);
        lock_table_release(&staff_lock);
        if (rc == 0) {
            char log[LOG_MSG_LEN];
            snprintf(log, sizeof(log), "PASSWORD_CHANGE kind=%s id=%d",
                     sess->staff_role == STAFF_MANAGER ? "MANAGER" : "EMPLOYEE", sess->staff_id);
            ipc_send_log(msqid, log);
        }
        send_frame(fd, OP_CHANGE_PW, rc == 0 ? "OK|password changed" : "ERR|write failed");
        return;
    }

    /* SESSION_ADMIN */
    RecordLockHandle admin_lock;
    if (lock_table_acquire(LOCK_RECORD_ADMIN, sess->admin_id, &admin_lock) != 0) {
        send_frame(fd, OP_CHANGE_PW, "ERR|account unavailable"); return;
    }
    rwlock_acquire_write(admin_lock.lock);
    Admin adm;
    if (storage_read_admin(sess->admin_id, &adm) != 0 || adm.status != STATUS_ACTIVE) {
        rwlock_release_write(admin_lock.lock);
        lock_table_release(&admin_lock);
        send_frame(fd, OP_CHANGE_PW, "ERR|account unavailable");
        return;
    }
    if (adm.pw_hash != old_hash) {
        rwlock_release_write(admin_lock.lock);
        lock_table_release(&admin_lock);
        send_frame(fd, OP_CHANGE_PW, "ERR|old password incorrect"); return;
    }
    adm.pw_hash = new_hash;
    int rc = storage_write_admin(&adm);
    rwlock_release_write(admin_lock.lock);
    lock_table_release(&admin_lock);
    if (rc == 0) {
        char log[LOG_MSG_LEN];
        snprintf(log, sizeof(log), "PASSWORD_CHANGE kind=ADMIN id=%d", sess->admin_id);
        ipc_send_log(msqid, log);
    }
    send_frame(fd, OP_CHANGE_PW, rc == 0 ? "OK|password changed" : "ERR|write failed");
}


int auth_dispatch(int fd, unsigned char opcode, char *payload, ClientSession *session) {
    if (opcode == OP_LOGIN) { handle_login(fd, payload, session, 0); return 1; }
    if (opcode == OP_CHANGE_PW) { handle_change_pw(fd, payload, session, 0); return 1; }
    return 0;
}
