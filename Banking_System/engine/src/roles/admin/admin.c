#include "common.h"
#include "protocol.h"
#include "storage.h"
#include "lock_table.h"
#include "session.h"
#include "util.h"
#include "role_access.h"
#include "response_format.h"
#include "session_guard.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ipc_send_log(unused, ...) ((void)(unused))
void handle_admin_add(int fd, char *payload, ClientSession *sess, int msqid) {
    if (!role_is_employee(sess)) { send_frame(fd, OP_ADMIN_ADD, "ERR|not authorized"); return; }
    /* role|owner1_name|owner1_pw|owner2_name|owner2_pw|initial_balance */
    char *fields[6];
    if (count_char(payload, '|') != 5 || split_fields(payload, fields, 6) != 6) {
        send_frame(fd, OP_ADMIN_ADD, "ERR|malformed request");
        return;
    }
    int is_joint = strcmp(fields[0], "JOINT") == 0;

    if (has_reserved_char(fields[1]) || has_reserved_char(fields[2]) ||
        (is_joint && (has_reserved_char(fields[3]) || has_reserved_char(fields[4])))) {
        send_frame(fd, OP_ADMIN_ADD, "ERR|names/passwords must not contain '|' or ';'");
        return;
    }
    double balance;
    if (!parse_amount(fields[5], &balance, 0)) {
        send_frame(fd, OP_ADMIN_ADD, "ERR|invalid initial balance");
        return;
    }

    /* find-a-slot and claim-it must be one atomic step: the per-record
     * RWLock can't cover the search (you don't know which record to lock
     * until the search is done), so without this two concurrent creations
     * both pick the same free slot and the second silently overwrites the
     * first. Lock order is always creation-mutex -> account RWLock. */
    lock_table_creation_lock();

    int id = storage_find_free_slot();
    if (id < 0) {
        lock_table_creation_unlock();
        send_frame(fd, OP_ADMIN_ADD, "ERR|no free account slots");
        return;
    }

    RecordLockHandle account_lock;
    if (lock_table_acquire(LOCK_RECORD_ACCOUNT, id, &account_lock) != 0) {
        lock_table_creation_unlock();
        send_frame(fd, OP_ADMIN_ADD, "ERR|could not allocate account lock"); return;
    }
    rwlock_acquire_write(account_lock.lock);
    Account acc;
    memset(&acc, 0, sizeof(acc));
    acc.account_id = id;
    acc.role = is_joint ? ROLE_JOINT : ROLE_NORMAL;
    acc.status = STATUS_ACTIVE;
    snprintf(acc.owner1_name, NAME_LEN, "%s", fields[1]);
    acc.owner1_pw_hash = simple_hash(fields[2]);
    if (is_joint) {
        snprintf(acc.owner2_name, NAME_LEN, "%s", fields[3]);
        acc.owner2_pw_hash = simple_hash(fields[4]);
    }
    acc.balance = balance;
    int rc = storage_write_account(&acc);
    rwlock_release_write(account_lock.lock);
    lock_table_release(&account_lock);
    lock_table_creation_unlock();

    if (rc == 0) {
        char log[LOG_MSG_LEN];
        snprintf(log, sizeof(log), "ADMIN_ADD by=%d new_account=%d role=%s", role_actor_id(sess), id, is_joint ? "JOINT" : "NORMAL");
        ipc_send_log(msqid, log);
    }

    char resp[64];
    snprintf(resp, sizeof(resp), rc == 0 ? "OK|%d" : "ERR|write failed", id);
    send_frame(fd, OP_ADMIN_ADD, resp);
}

void handle_admin_delete(int fd, char *payload, ClientSession *sess, int msqid) {
    /* Employees manage the complete customer-account lifecycle described
     * in the brief: opening, modifying, and closing accounts.  Closing is
     * a soft delete, so the record (and its audit history) is retained. */
    if (!role_is_employee(sess)) { send_frame(fd, OP_ADMIN_DELETE, "ERR|not authorized"); return; }
    int id = atoi(payload);
    RecordLockHandle account_lock;
    if (lock_table_acquire(LOCK_RECORD_ACCOUNT, id, &account_lock) != 0) {
        send_frame(fd, OP_ADMIN_DELETE, "ERR|invalid account id"); return;
    }

    rwlock_acquire_write(account_lock.lock);
    Account acc;
    if (storage_read_account(id, &acc) != 0 || acc.status == STATUS_EMPTY || acc.status == STATUS_DELETED) {
        rwlock_release_write(account_lock.lock);
        lock_table_release(&account_lock);
        send_frame(fd, OP_ADMIN_DELETE, "ERR|account not found");
        return;
    }
    acc.status = STATUS_DELETED;
    int rc = storage_write_account(&acc);
    rwlock_release_write(account_lock.lock);
    lock_table_release(&account_lock);

    if (rc == 0) {
        char log[LOG_MSG_LEN];
        snprintf(log, sizeof(log), "ACCOUNT_CLOSE actor=%d account=%d", role_actor_id(sess), id);
        ipc_send_log(msqid, log);
    }
    send_frame(fd, OP_ADMIN_DELETE, rc == 0 ? "OK|deleted" : "ERR|write failed");
}

void handle_admin_modify(int fd, char *payload, ClientSession *sess, int msqid) {
    if (!role_is_employee(sess)) { send_frame(fd, OP_ADMIN_MODIFY, "ERR|not authorized"); return; }
    /* account_id|field|value ; field in {balance, owner1_name, owner2_name} */
    char *fields[3];
    if (count_char(payload, '|') != 2 || split_fields(payload, fields, 3) != 3) {
        send_frame(fd, OP_ADMIN_MODIFY, "ERR|malformed request");
        return;
    }
    int id = atoi(fields[0]);
    RecordLockHandle account_lock;
    if (lock_table_acquire(LOCK_RECORD_ACCOUNT, id, &account_lock) != 0) {
        send_frame(fd, OP_ADMIN_MODIFY, "ERR|invalid account id"); return;
    }

    rwlock_acquire_write(account_lock.lock);
    Account acc;
    if (storage_read_account(id, &acc) != 0 || acc.status != STATUS_ACTIVE) {
        rwlock_release_write(account_lock.lock);
        lock_table_release(&account_lock);
        send_frame(fd, OP_ADMIN_MODIFY, "ERR|account not found");
        return;
    }
    int ok = 1;
    const char *err = "unknown field or write failed";
    if (strcmp(fields[1], "balance") == 0) {
        double v;
        if (!parse_amount(fields[2], &v, 0)) { ok = 0; err = "invalid balance value"; }
        else acc.balance = v;
    } else if (strcmp(fields[1], "owner1_name") == 0) {
        if (has_reserved_char(fields[2])) { ok = 0; err = "name must not contain '|' or ';'"; }
        else snprintf(acc.owner1_name, NAME_LEN, "%s", fields[2]);
    } else if (strcmp(fields[1], "owner2_name") == 0) {
        if (has_reserved_char(fields[2])) { ok = 0; err = "name must not contain '|' or ';'"; }
        else snprintf(acc.owner2_name, NAME_LEN, "%s", fields[2]);
    } else {
        ok = 0;
    }
    int rc = ok ? storage_write_account(&acc) : -1;
    rwlock_release_write(account_lock.lock);
    lock_table_release(&account_lock);

    if (ok && rc == 0) {
        char log[LOG_MSG_LEN];
        snprintf(log, sizeof(log), "ADMIN_MODIFY by=%d account=%d field=%s", role_actor_id(sess), id, fields[1]);
        ipc_send_log(msqid, log);
        send_frame(fd, OP_ADMIN_MODIFY, "OK|modified");
    } else {
        char resp[LOG_MSG_LEN];
        snprintf(resp, sizeof(resp), "ERR|%s", err);
        send_frame(fd, OP_ADMIN_MODIFY, resp);
    }
}


void handle_staff_add(int fd, char *payload, ClientSession *sess, int msqid) {
    if (!role_is_admin(sess)) { send_frame(fd, OP_STAFF_ADD, "ERR|not authorized"); return; }
    /* role|name|password */
    char *fields[3];
    if (count_char(payload, '|') != 2 || split_fields(payload, fields, 3) != 3) {
        send_frame(fd, OP_STAFF_ADD, "ERR|malformed request, expected role|name|password");
        return;
    }
    StaffRole role;
    if (strcmp(fields[0], "EMPLOYEE") == 0) role = STAFF_EMPLOYEE;
    else if (strcmp(fields[0], "MANAGER") == 0) role = STAFF_MANAGER;
    else { send_frame(fd, OP_STAFF_ADD, "ERR|role must be EMPLOYEE or MANAGER"); return; }

    if (has_reserved_char(fields[1]) || has_reserved_char(fields[2])) {
        send_frame(fd, OP_STAFF_ADD, "ERR|name/password must not contain '|' or ';'");
        return;
    }

    Staff st;
    memset(&st, 0, sizeof(st));
    st.role = role;
    st.status = STATUS_ACTIVE;
    snprintf(st.name, NAME_LEN, "%s", fields[1]);
    st.pw_hash = simple_hash(fields[2]);
    if (storage_add_staff(&st) != 0) { send_frame(fd, OP_STAFF_ADD, "ERR|no free staff slots"); return; }

    char log[LOG_MSG_LEN];
    snprintf(log, sizeof(log), "STAFF_ADD admin=%d new_staff=%d role=%s", sess->admin_id, st.staff_id, fields[0]);
    ipc_send_log(msqid, log);

    char resp[64];
    snprintf(resp, sizeof(resp), "OK|%d", st.staff_id);
    send_frame(fd, OP_STAFF_ADD, resp);
}

void handle_staff_modify(int fd, char *payload, ClientSession *sess, int msqid) {
    if (!role_is_admin(sess)) { send_frame(fd, OP_STAFF_MODIFY, "ERR|not authorized"); return; }
    /* role|staff_id|field|value */
    char *fields[4];
    if (count_char(payload, '|') != 3 || split_fields(payload, fields, 4) != 4) {
        send_frame(fd, OP_STAFF_MODIFY, "ERR|malformed request, expected role|staff_id|field|value");
        return;
    }
    StaffRole role;
    if (strcmp(fields[0], "EMPLOYEE") == 0) role = STAFF_EMPLOYEE;
    else if (strcmp(fields[0], "MANAGER") == 0) role = STAFF_MANAGER;
    else { send_frame(fd, OP_STAFF_MODIFY, "ERR|role must be EMPLOYEE or MANAGER"); return; }
    int id = atoi(fields[1]);
    RecordLockHandle staff_lock;
    if (lock_table_acquire(LOCK_RECORD_STAFF, storage_staff_lock_id(role, id), &staff_lock) != 0) {
        send_frame(fd, OP_STAFF_MODIFY, "ERR|invalid staff id"); return;
    }
    rwlock_acquire_write(staff_lock.lock);
    Staff st;
    if (storage_read_staff(role, id, &st) != 0 || st.status == STATUS_EMPTY || st.status == STATUS_DELETED) {
        rwlock_release_write(staff_lock.lock);
        lock_table_release(&staff_lock);
        send_frame(fd, OP_STAFF_MODIFY, "ERR|staff not found");
        return;
    }
    int ok = 1;
    const char *err = "unknown field";
    int role_change = 0;
    if (strcmp(fields[2], "role") == 0) {
        StaffRole new_role;
        if (strcmp(fields[3], "EMPLOYEE") == 0) new_role = STAFF_EMPLOYEE;
        else if (strcmp(fields[3], "MANAGER") == 0) new_role = STAFF_MANAGER;
        else { ok = 0; err = "role must be EMPLOYEE or MANAGER"; new_role = role; }
        role_change = ok && new_role != role;
    } else if (strcmp(fields[2], "name") == 0) {
        if (has_reserved_char(fields[3])) { ok = 0; err = "name must not contain '|' or ';'"; }
        else snprintf(st.name, NAME_LEN, "%s", fields[3]);
    } else if (strcmp(fields[2], "status") == 0) {
        if (strcmp(fields[3], "ACTIVE") == 0) st.status = STATUS_ACTIVE;
        else if (strcmp(fields[3], "DELETED") == 0) st.status = STATUS_DELETED;
        else { ok = 0; err = "status must be ACTIVE or DELETED"; }
    } else {
        ok = 0;
    }
    int rc = -1;
    if (ok && role_change) {
        int guard_id = storage_staff_lock_id(role, id);
        if (!session_guard_acquire(GUARD_STAFF, guard_id, 0)) {
            rwlock_release_write(staff_lock.lock);
            lock_table_release(&staff_lock);
            send_frame(fd, OP_STAFF_MODIFY, "ERR|cannot change role while staff member is logged in");
            return;
        }
        rwlock_release_write(staff_lock.lock);
        lock_table_release(&staff_lock);
        StaffRole new_role = strcmp(fields[3], "MANAGER") == 0 ? STAFF_MANAGER : STAFF_EMPLOYEE;
        rc = storage_change_staff_role(role, id, new_role);
        session_guard_release(GUARD_STAFF, guard_id, 0);
    } else {
        if (ok) rc = storage_update_staff(role, &st);
        rwlock_release_write(staff_lock.lock);
        lock_table_release(&staff_lock);
    }

    if (ok && rc == 0) {
        char log[LOG_MSG_LEN];
        snprintf(log, sizeof(log), "STAFF_MODIFY admin=%d role=%s staff=%d field=%s",
                 sess->admin_id, fields[0], id, fields[2]);
        ipc_send_log(msqid, log);
        send_frame(fd, OP_STAFF_MODIFY, "OK|modified");
    } else {
        if (ok && role_change) err = "could not change staff role";
        char resp[LOG_MSG_LEN];
        snprintf(resp, sizeof(resp), "ERR|%s", err);
        send_frame(fd, OP_STAFF_MODIFY, resp);
    }
}


int admin_dispatch(int fd, unsigned char opcode, char *payload, ClientSession *session) {
    switch ((Opcode)opcode) {
        case OP_ADMIN_ADD: handle_admin_add(fd, payload, session, 0); return 1;
        case OP_ADMIN_DELETE: handle_admin_delete(fd, payload, session, 0); return 1;
        case OP_ADMIN_MODIFY: handle_admin_modify(fd, payload, session, 0); return 1;
        case OP_STAFF_ADD: handle_staff_add(fd, payload, session, 0); return 1;
        case OP_STAFF_MODIFY: handle_staff_modify(fd, payload, session, 0); return 1;
        default: return 0;
    }
}
