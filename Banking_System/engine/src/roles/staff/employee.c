#include "common.h"
#include "protocol.h"
#include "storage.h"
#include "lock_table.h"
#include "session.h"
#include "util.h"
#include "role_access.h"
#include "response_format.h"
#include "journal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ipc_send_log(unused, ...) ((void)(unused))

typedef struct { ListBuilder *lb; int employee_id; } AssignedLoanList;

static int append_assigned_loan(const Loan *ln, void *ctx_v) {
    AssignedLoanList *ctx = ctx_v;
    /* This is an Employee work queue, not a decision audit log. Once a loan
     * is approved/rejected it must not be presented as actionable again. */
    if (ln->status == LOAN_PENDING &&
        (ctx->employee_id < 0 || ln->assigned_employee_id == ctx->employee_id)) {
        char entry[320];
        fmt_loan(ln, entry, sizeof(entry));
        list_builder_add(ctx->lb, entry);
    }
    return 1;
}

void handle_loan_assigned_list(int fd, ClientSession *sess) {
    if (!role_is_employee(sess)) { send_frame(fd, OP_LOAN_ASSIGNED_LIST, "ERR|not authorized"); return; }
    ListBuilder lb;
    list_builder_init(&lb, (size_t)MAX_LOANS * 200 + 16);
    if (!lb.buf) { send_frame(fd, OP_LOAN_ASSIGNED_LIST, "ERR|out of memory"); return; }
    AssignedLoanList ctx = { &lb, sess->role == SESSION_ADMIN ? -1 : sess->staff_id };
    storage_for_each_loan(append_assigned_loan, &ctx);
    list_builder_respond(fd, OP_LOAN_ASSIGNED_LIST, &lb);
}

void handle_loan_decide(int fd, char *payload, ClientSession *sess, int msqid) {
    if (!role_is_employee(sess)) { send_frame(fd, OP_LOAN_DECIDE, "ERR|not authorized"); return; }
    char *fields[2];
    if (count_char(payload, '|') != 1 || split_fields(payload, fields, 2) != 2) {
        send_frame(fd, OP_LOAN_DECIDE, "ERR|malformed request, expected loan_id|APPROVE or REJECT");
        return;
    }
    int loan_id = atoi(fields[0]);
    RecordLockHandle loan_lock;
    if (lock_table_acquire(LOCK_RECORD_LOAN, loan_id, &loan_lock) != 0) {
        send_frame(fd, OP_LOAN_DECIDE, "ERR|invalid loan id"); return;
    }
    rwlock_acquire_write(loan_lock.lock);
    Loan ln;
    if (storage_read_loan(loan_id, &ln) != 0 || ln.status == LOAN_EMPTY) {
        rwlock_release_write(loan_lock.lock); lock_table_release(&loan_lock);
        send_frame(fd, OP_LOAN_DECIDE, "ERR|loan not found"); return;
    }
    if (ln.status != LOAN_PENDING) {
        rwlock_release_write(loan_lock.lock); lock_table_release(&loan_lock);
        send_frame(fd, OP_LOAN_DECIDE, "ERR|loan already decided"); return;
    }
    Loan loan_before = ln;
    if (sess->role != SESSION_ADMIN && ln.assigned_employee_id != sess->staff_id) {
        rwlock_release_write(loan_lock.lock); lock_table_release(&loan_lock);
        send_frame(fd, OP_LOAN_DECIDE, "ERR|loan is not assigned to you"); return;
    }
    int approve = strcmp(fields[1], "APPROVE") == 0;
    int reject = strcmp(fields[1], "REJECT") == 0;
    if (!approve && !reject) {
        rwlock_release_write(loan_lock.lock); lock_table_release(&loan_lock);
        send_frame(fd, OP_LOAN_DECIDE, "ERR|expected APPROVE or REJECT"); return;
    }
    Account before, after;
    RecordLockHandle account_lock = {0};
    if (approve) {
        if (lock_table_acquire(LOCK_RECORD_ACCOUNT, ln.account_id, &account_lock) != 0) {
            rwlock_release_write(loan_lock.lock); lock_table_release(&loan_lock);
            send_frame(fd, OP_LOAN_DECIDE, "ERR|applicant account unavailable"); return;
        }
        rwlock_acquire_write(account_lock.lock);
        if (storage_read_account(ln.account_id, &before) != 0 || before.status != STATUS_ACTIVE) {
            rwlock_release_write(account_lock.lock); lock_table_release(&account_lock);
            rwlock_release_write(loan_lock.lock); lock_table_release(&loan_lock);
            send_frame(fd, OP_LOAN_DECIDE, "ERR|applicant account unavailable"); return;
        }
        after = before;
        after.balance += ln.amount;
    }
    ln.status = approve ? LOAN_APPROVED : LOAN_REJECTED;
    ln.decided_by = role_actor_id(sess);
    int write_rc;
    if (approve) {
        LedgerEntry entry;
        memset(&entry, 0, sizeof(entry));
        fmt_timestamp(entry.timestamp, sizeof(entry.timestamp));
        entry.account_id = before.account_id;
        snprintf(entry.owner, sizeof(entry.owner), "%s", before.owner1_name);
        snprintf(entry.operation, sizeof(entry.operation), "LOAN_DISBURSEMENT");
        entry.amount = ln.amount;
        entry.balance_after = after.balance;
        write_rc = journal_approve_loan(&before, &after, &loan_before, &ln, &entry);
        rwlock_release_write(account_lock.lock);
        lock_table_release(&account_lock);
    } else {
        write_rc = storage_update_loan(&ln);
    }
    rwlock_release_write(loan_lock.lock);
    lock_table_release(&loan_lock);
    if (write_rc != 0) { send_frame(fd, OP_LOAN_DECIDE, "ERR|write failed"); return; }
    char log[LOG_MSG_LEN];
    snprintf(log, sizeof(log), "LOAN_DECIDE loan=%d by=%d decision=%s", loan_id, ln.decided_by, approve ? "APPROVED" : "REJECTED");
    ipc_send_log(msqid, log);
    send_frame(fd, OP_LOAN_DECIDE, approve ? "OK|approved" : "OK|rejected");
}

int employee_dispatch(int fd, unsigned char opcode, char *payload, ClientSession *session) {
    switch ((Opcode)opcode) {
        case OP_LOAN_ASSIGNED_LIST: handle_loan_assigned_list(fd, session); return 1;
        case OP_LOAN_DECIDE: handle_loan_decide(fd, payload, session, 0); return 1;
        default: return 0;
    }
}
