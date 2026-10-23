#include "common.h"
#include "protocol.h"
#include "storage.h"
#include "lock_table.h"
#include "session.h"
#include "util.h"
#include "role_access.h"
#include "response_format.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ipc_send_log(unused, ...) ((void)(unused))

static int append_feedback(const Feedback *fb, void *ctx_v) {
    ListBuilder *lb = ctx_v;
    char entry[320];
    snprintf(entry, sizeof(entry), "id=%d|account=%d|text=%s", fb->feedback_id, fb->account_id, fb->text);
    list_builder_add(lb, entry);
    return 1;
}

static int append_unassigned_pending_loan(const Loan *ln, void *ctx_v) {
    ListBuilder *lb = ctx_v;
    if (ln->status == LOAN_PENDING && ln->assigned_employee_id == 0) {
        char entry[320];
        fmt_loan(ln, entry, sizeof(entry));
        list_builder_add(lb, entry);
    }
    return 1;
}

static int append_all_loans(const Loan *ln, void *ctx_v) {
    ListBuilder *lb = ctx_v;
    if (ln->status != LOAN_EMPTY) {
        char entry[400];
        fmt_loan_history(ln, entry, sizeof(entry));
        list_builder_add(lb, entry);
    }
    return 1;
}

void handle_loan_decision_history(int fd, ClientSession *sess) {
    if (!role_is_staff_any(sess)) { send_frame(fd, OP_LOAN_DECISION_HISTORY, "ERR|not authorized"); return; }
    ListBuilder lb;
    list_builder_init(&lb, (size_t)MAX_LOANS * 400 + 16);
    if (!lb.buf) { send_frame(fd, OP_LOAN_DECISION_HISTORY, "ERR|out of memory"); return; }
    storage_for_each_loan(append_all_loans, &lb);
    list_builder_respond(fd, OP_LOAN_DECISION_HISTORY, &lb);
}

/* A newly submitted loan stays in this Manager queue until it is assigned.
 * Employees cannot see it before assignment. */
void handle_loan_pending_list(int fd, ClientSession *sess) {
    if (!role_is_manager(sess)) { send_frame(fd, OP_LOAN_PENDING_LIST, "ERR|not authorized"); return; }
    ListBuilder lb;
    list_builder_init(&lb, (size_t)MAX_LOANS * 200 + 16);
    if (!lb.buf) { send_frame(fd, OP_LOAN_PENDING_LIST, "ERR|out of memory"); return; }
    storage_for_each_loan(append_unassigned_pending_loan, &lb);
    list_builder_respond(fd, OP_LOAN_PENDING_LIST, &lb);
}

void handle_loan_assign(int fd, char *payload, ClientSession *sess, int msqid) {
    if (!role_is_manager(sess)) { send_frame(fd, OP_LOAN_ASSIGN, "ERR|not authorized"); return; }
    char *fields[2];
    if (count_char(payload, '|') != 1 || split_fields(payload, fields, 2) != 2) {
        send_frame(fd, OP_LOAN_ASSIGN, "ERR|malformed request, expected loan_id|employee_id"); return;
    }
    int loan_id = atoi(fields[0]);
    int employee_id = atoi(fields[1]);
    RecordLockHandle loan_lock;
    if (lock_table_acquire(LOCK_RECORD_LOAN, loan_id, &loan_lock) != 0) {
        send_frame(fd, OP_LOAN_ASSIGN, "ERR|invalid loan id"); return;
    }
    rwlock_acquire_write(loan_lock.lock);
    Loan ln;
    if (storage_read_loan(loan_id, &ln) != 0 || ln.status == LOAN_EMPTY) {
        rwlock_release_write(loan_lock.lock); lock_table_release(&loan_lock);
        send_frame(fd, OP_LOAN_ASSIGN, "ERR|loan not found"); return;
    }
    if (ln.status != LOAN_PENDING) {
        rwlock_release_write(loan_lock.lock); lock_table_release(&loan_lock);
        send_frame(fd, OP_LOAN_ASSIGN, "ERR|loan already decided"); return;
    }
    if (ln.assigned_employee_id != 0) {
        rwlock_release_write(loan_lock.lock); lock_table_release(&loan_lock);
        send_frame(fd, OP_LOAN_ASSIGN, "ERR|loan is already assigned"); return;
    }
    Staff st;
    RecordLockHandle staff_lock;
    int staff_rc = lock_table_acquire(LOCK_RECORD_STAFF,
                                      storage_staff_lock_id(STAFF_EMPLOYEE, employee_id), &staff_lock);
    if (staff_rc == 0) rwlock_acquire_read(staff_lock.lock);
    int read_rc = staff_rc == 0 ? storage_read_staff(STAFF_EMPLOYEE, employee_id, &st) : -1;
    if (staff_rc == 0) { rwlock_release_read(staff_lock.lock); lock_table_release(&staff_lock); }
    if (read_rc != 0 || st.status != STATUS_ACTIVE || st.role != STAFF_EMPLOYEE) {
        rwlock_release_write(loan_lock.lock); lock_table_release(&loan_lock);
        send_frame(fd, OP_LOAN_ASSIGN, "ERR|invalid employee id"); return;
    }
    ln.assigned_employee_id = employee_id;
    int write_rc = storage_update_loan(&ln);
    rwlock_release_write(loan_lock.lock);
    lock_table_release(&loan_lock);
    if (write_rc != 0) { send_frame(fd, OP_LOAN_ASSIGN, "ERR|write failed"); return; }
    char log[LOG_MSG_LEN];
    snprintf(log, sizeof(log), "LOAN_ASSIGN loan=%d employee=%d by=%d", loan_id, employee_id, role_actor_id(sess));
    ipc_send_log(msqid, log);
    send_frame(fd, OP_LOAN_ASSIGN, "OK|assigned");
}

void handle_feedback_list(int fd, ClientSession *sess) {
    if (!role_is_manager(sess)) { send_frame(fd, OP_FEEDBACK_LIST, "ERR|not authorized"); return; }
    ListBuilder lb;
    list_builder_init(&lb, (size_t)MAX_FEEDBACK * 300 + 16);
    if (!lb.buf) { send_frame(fd, OP_FEEDBACK_LIST, "ERR|out of memory"); return; }
    storage_for_each_feedback(append_feedback, &lb);
    list_builder_respond(fd, OP_FEEDBACK_LIST, &lb);
}

void handle_account_set_status(int fd, char *payload, ClientSession *sess, int msqid) {
    if (!role_is_manager(sess)) { send_frame(fd, OP_ACCOUNT_SET_STATUS, "ERR|not authorized"); return; }
    char *fields[2];
    if (count_char(payload, '|') != 1 || split_fields(payload, fields, 2) != 2) {
        send_frame(fd, OP_ACCOUNT_SET_STATUS, "ERR|malformed request, expected account_id|ACTIVE or INACTIVE"); return;
    }
    int id = atoi(fields[0]);
    int activate = strcmp(fields[1], "ACTIVE") == 0;
    int deactivate = strcmp(fields[1], "INACTIVE") == 0;
    if (!activate && !deactivate) { send_frame(fd, OP_ACCOUNT_SET_STATUS, "ERR|expected ACTIVE or INACTIVE"); return; }
    RecordLockHandle account_lock;
    if (lock_table_acquire(LOCK_RECORD_ACCOUNT, id, &account_lock) != 0) {
        send_frame(fd, OP_ACCOUNT_SET_STATUS, "ERR|invalid account id"); return;
    }
    rwlock_acquire_write(account_lock.lock);
    Account acc;
    if (storage_read_account(id, &acc) != 0 || (acc.status != STATUS_ACTIVE && acc.status != STATUS_INACTIVE)) {
        rwlock_release_write(account_lock.lock); lock_table_release(&account_lock);
        send_frame(fd, OP_ACCOUNT_SET_STATUS, "ERR|account not found or not eligible"); return;
    }
    acc.status = activate ? STATUS_ACTIVE : STATUS_INACTIVE;
    int rc = storage_write_account(&acc);
    rwlock_release_write(account_lock.lock);
    lock_table_release(&account_lock);
    if (rc == 0) {
        char log[LOG_MSG_LEN];
        snprintf(log, sizeof(log), "ACCOUNT_STATUS account=%d status=%s by=%d", id, activate ? "ACTIVE" : "INACTIVE", role_actor_id(sess));
        ipc_send_log(msqid, log);
    }
    send_frame(fd, OP_ACCOUNT_SET_STATUS, rc == 0 ? "OK|updated" : "ERR|write failed");
}

void handle_staff_list(int fd, ClientSession *sess) {
    if (!role_is_manager(sess)) { send_frame(fd, OP_STAFF_LIST, "ERR|not authorized"); return; }
    ListBuilder lb;
    list_builder_init(&lb, (size_t)MAX_STAFF * 2 * 160 + 16);
    if (!lb.buf) { send_frame(fd, OP_STAFF_LIST, "ERR|out of memory"); return; }
    const StaffRole roles[] = { STAFF_EMPLOYEE, STAFF_MANAGER };
    for (size_t role_index = 0; role_index < sizeof(roles) / sizeof(roles[0]); role_index++) {
        for (int id = 1; id <= MAX_STAFF; id++) {
            Staff st;
            RecordLockHandle staff_lock;
            int lock_id = storage_staff_lock_id(roles[role_index], id);
            if (lock_table_acquire(LOCK_RECORD_STAFF, lock_id, &staff_lock) != 0) continue;
            rwlock_acquire_read(staff_lock.lock);
            int rc = storage_read_staff(roles[role_index], id, &st);
            rwlock_release_read(staff_lock.lock);
            lock_table_release(&staff_lock);
            if (rc != 0 || st.status == STATUS_EMPTY) continue;
            char entry[200];
            fmt_staff(&st, entry, sizeof(entry));
            list_builder_add(&lb, entry);
        }
    }
    list_builder_respond(fd, OP_STAFF_LIST, &lb);
}

int manager_dispatch(int fd, unsigned char opcode, char *payload, ClientSession *session) {
    switch ((Opcode)opcode) {
        case OP_LOAN_ASSIGN: handle_loan_assign(fd, payload, session, 0); return 1;
        case OP_LOAN_PENDING_LIST: handle_loan_pending_list(fd, session); return 1;
        case OP_LOAN_DECISION_HISTORY: handle_loan_decision_history(fd, session); return 1;
        case OP_FEEDBACK_LIST: handle_feedback_list(fd, session); return 1;
        case OP_ACCOUNT_SET_STATUS: handle_account_set_status(fd, payload, session, 0); return 1;
        case OP_STAFF_LIST: handle_staff_list(fd, session); return 1;
        default: return 0;
    }
}
