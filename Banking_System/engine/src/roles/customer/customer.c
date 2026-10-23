#include "common.h"
#include "protocol.h"
#include "storage.h"
#include "lock_table.h"
#include "journal.h"
#include "session.h"
#include "util.h"
#include "response_format.h"
#include "role_access.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ipc_send_log(unused, ...) ((void)(unused))
void handle_deposit(int fd, char *payload, ClientSession *sess, int msqid) {
    (void)msqid;
    if (sess->role != SESSION_USER) { send_frame(fd, OP_DEPOSIT, "ERR|not logged in"); return; }
    double amount;
    if (!parse_amount(payload, &amount, 0) || amount <= 0) {
        send_frame(fd, OP_DEPOSIT, "ERR|invalid amount");
        return;
    }

    RecordLockHandle account_lock;
    if (lock_table_acquire(LOCK_RECORD_ACCOUNT, sess->account_id, &account_lock) != 0) {
        send_frame(fd, OP_DEPOSIT, "ERR|account unavailable"); return;
    }
    rwlock_acquire_write(account_lock.lock);
    Account before, acc;
    if (storage_read_account(sess->account_id, &before) != 0 || before.status != STATUS_ACTIVE) {
        rwlock_release_write(account_lock.lock);
        lock_table_release(&account_lock);
        send_frame(fd, OP_DEPOSIT, "ERR|account unavailable");
        return;
    }
    acc = before;
    acc.balance += amount;
    LedgerEntry entry;
    memset(&entry, 0, sizeof(entry));
    fmt_timestamp(entry.timestamp, sizeof(entry.timestamp));
    entry.account_id = acc.account_id;
    snprintf(entry.owner, sizeof(entry.owner), "%s", sess->as_owner2 ? acc.owner2_name : acc.owner1_name);
    snprintf(entry.operation, sizeof(entry.operation), "DEPOSIT");
    entry.amount = amount;
    entry.balance_after = acc.balance;
    int rc = journal_commit_financial(&before, &acc, NULL, NULL, &entry, NULL);
    rwlock_release_write(account_lock.lock);
    lock_table_release(&account_lock);

    if (rc != 0) { send_frame(fd, OP_DEPOSIT, "ERR|write failed"); return; }

    char resp[64];
    snprintf(resp, sizeof(resp), "OK|%.2f", acc.balance);
    send_frame(fd, OP_DEPOSIT, resp);
}

void handle_withdraw(int fd, char *payload, ClientSession *sess, int msqid) {
    (void)msqid;
    if (sess->role != SESSION_USER) { send_frame(fd, OP_WITHDRAW, "ERR|not logged in"); return; }
    double amount;
    if (!parse_amount(payload, &amount, 0) || amount <= 0) {
        send_frame(fd, OP_WITHDRAW, "ERR|invalid amount");
        return;
    }

    RecordLockHandle account_lock;
    if (lock_table_acquire(LOCK_RECORD_ACCOUNT, sess->account_id, &account_lock) != 0) {
        send_frame(fd, OP_WITHDRAW, "ERR|account unavailable"); return;
    }
    rwlock_acquire_write(account_lock.lock);
    Account before, acc;
    if (storage_read_account(sess->account_id, &before) != 0 || before.status != STATUS_ACTIVE) {
        rwlock_release_write(account_lock.lock);
        lock_table_release(&account_lock);
        send_frame(fd, OP_WITHDRAW, "ERR|account unavailable");
        return;
    }
    if (before.balance < amount) {
        rwlock_release_write(account_lock.lock);
        lock_table_release(&account_lock);
        send_frame(fd, OP_WITHDRAW, "ERR|insufficient funds");
        return;
    }
    acc = before;
    acc.balance -= amount;
    LedgerEntry entry;
    memset(&entry, 0, sizeof(entry));
    fmt_timestamp(entry.timestamp, sizeof(entry.timestamp));
    entry.account_id = acc.account_id;
    snprintf(entry.owner, sizeof(entry.owner), "%s", sess->as_owner2 ? acc.owner2_name : acc.owner1_name);
    snprintf(entry.operation, sizeof(entry.operation), "WITHDRAW");
    entry.amount = amount;
    entry.balance_after = acc.balance;
    int rc = journal_commit_financial(&before, &acc, NULL, NULL, &entry, NULL);
    rwlock_release_write(account_lock.lock);
    lock_table_release(&account_lock);

    if (rc != 0) { send_frame(fd, OP_WITHDRAW, "ERR|write failed"); return; }

    char resp[64];
    snprintf(resp, sizeof(resp), "OK|%.2f", acc.balance);
    send_frame(fd, OP_WITHDRAW, resp);
}

/* Atomic funds transfer between two accounts -- the ACID centerpiece.
 *
 * Atomicity + Isolation: both accounts' write locks are held for the
 * *entire* validate-then-write critical section, so either both balances
 * change or neither does, and no other operation on either account can
 * be interleaved in between.
 *
 * Deadlock avoidance: locks are always acquired in ascending account-id
 * order, regardless of transfer direction. Two concurrent transfers A->B
 * and B->A therefore always agree on which lock to take first
 * (min(A,B)), so a circular wait can never form -- proven directly in
 * tests/test_engine.py with hundreds of transfers fired in both
 * directions simultaneously.
 *
 * Consistency: the sum of both balances is unchanged by construction
 * (one side's debit is exactly the other side's credit); the write
 * happens only after both accounts are confirmed active and the source
 * has sufficient funds.
 *
 * Durability: storage_write_account() calls fdatasync() after every
 * write (see storage.c), so a committed leg of the transfer survives a
 * crash immediately after this call returns.
 */
void handle_transfer(int fd, char *payload, ClientSession *sess, int msqid) {
    (void)msqid;
    if (sess->role != SESSION_USER) { send_frame(fd, OP_TRANSFER, "ERR|not logged in"); return; }
    char *fields[2];
    if (count_char(payload, '|') != 1 || split_fields(payload, fields, 2) != 2) {
        send_frame(fd, OP_TRANSFER, "ERR|malformed request, expected to_account_id|amount");
        return;
    }
    int to_id = atoi(fields[0]);
    double amount;
    if (!parse_amount(fields[1], &amount, 0) || amount <= 0) {
        send_frame(fd, OP_TRANSFER, "ERR|invalid amount");
        return;
    }
    int from_id = sess->account_id;
    if (to_id <= 0 || to_id > MAX_ACCOUNTS) { send_frame(fd, OP_TRANSFER, "ERR|invalid destination account"); return; }
    if (to_id == from_id) { send_frame(fd, OP_TRANSFER, "ERR|cannot transfer to the same account"); return; }

    RecordLockHandle from_handle = {0}, to_handle = {0};
    if (lock_table_acquire(LOCK_RECORD_ACCOUNT, from_id, &from_handle) != 0 ||
        lock_table_acquire(LOCK_RECORD_ACCOUNT, to_id, &to_handle) != 0) {
        lock_table_release(&from_handle);
        send_frame(fd, OP_TRANSFER, "ERR|account unavailable");
        return;
    }
    RWLock *first_lock = (from_id < to_id) ? from_handle.lock : to_handle.lock;
    RWLock *second_lock = (from_id < to_id) ? to_handle.lock : from_handle.lock;

    rwlock_acquire_write(first_lock);
    rwlock_acquire_write(second_lock);

    Account before_from, before_to, from_acc, to_acc;
    int rc_from = storage_read_account(from_id, &before_from);
    int rc_to = storage_read_account(to_id, &before_to);

    const char *err = NULL;
    if (rc_from != 0 || before_from.status != STATUS_ACTIVE) err = "source account unavailable";
    else if (rc_to != 0 || before_to.status != STATUS_ACTIVE) err = "destination account unavailable";
    else if (before_from.balance < amount) err = "insufficient funds";

    if (err) {
        rwlock_release_write(second_lock);
        rwlock_release_write(first_lock);
        lock_table_release(&to_handle);
        lock_table_release(&from_handle);
        char resp[LOG_MSG_LEN];
        snprintf(resp, sizeof(resp), "ERR|%s", err);
        send_frame(fd, OP_TRANSFER, resp);
        return;
    }

    from_acc = before_from;
    to_acc = before_to;
    from_acc.balance -= amount;
    to_acc.balance += amount;
    LedgerEntry out, in;
    memset(&out, 0, sizeof(out));
    memset(&in, 0, sizeof(in));
    fmt_timestamp(out.timestamp, sizeof(out.timestamp));
    snprintf(in.timestamp, sizeof(in.timestamp), "%s", out.timestamp);
    out.account_id = from_id; in.account_id = to_id;
    snprintf(out.owner, sizeof(out.owner), "%s", sess->as_owner2 ? from_acc.owner2_name : from_acc.owner1_name);
    snprintf(in.owner, sizeof(in.owner), "%s", to_acc.owner1_name);
    snprintf(out.operation, sizeof(out.operation), "TRANSFER_OUT");
    snprintf(in.operation, sizeof(in.operation), "TRANSFER_IN");
    out.amount = in.amount = amount;
    out.balance_after = from_acc.balance; in.balance_after = to_acc.balance;
    out.counterparty_account_id = to_id; in.counterparty_account_id = from_id;
    int rc1 = journal_commit_financial(&before_from, &from_acc, &before_to, &to_acc, &out, &in);

    rwlock_release_write(second_lock);
    rwlock_release_write(first_lock);
    lock_table_release(&to_handle);
    lock_table_release(&from_handle);

    if (rc1 != 0) {
        send_frame(fd, OP_TRANSFER, "ERR|durable transaction failed; no transfer committed");
        return;
    }

    char resp[64];
    snprintf(resp, sizeof(resp), "OK|%.2f", from_acc.balance);
    send_frame(fd, OP_TRANSFER, resp);
}

void handle_balance(int fd, ClientSession *sess) {
    if (sess->role != SESSION_USER) { send_frame(fd, OP_BALANCE, "ERR|not logged in"); return; }
    RecordLockHandle account_lock;
    if (lock_table_acquire(LOCK_RECORD_ACCOUNT, sess->account_id, &account_lock) != 0) {
        send_frame(fd, OP_BALANCE, "ERR|account unavailable"); return;
    }
    rwlock_acquire_read(account_lock.lock);
    Account acc;
    int rc = storage_read_account(sess->account_id, &acc);
    rwlock_release_read(account_lock.lock);
    lock_table_release(&account_lock);

    if (rc != 0) { send_frame(fd, OP_BALANCE, "ERR|account unavailable"); return; }
    char resp[64];
    snprintf(resp, sizeof(resp), "OK|%.2f", acc.balance);
    send_frame(fd, OP_BALANCE, resp);
}

/* Change Password -- every role has this, so it branches on session role
 * rather than being customer-only. */

void handle_view_details(int fd, ClientSession *sess) {
    if (sess->role != SESSION_USER) { send_frame(fd, OP_VIEW_DETAILS, "ERR|not logged in"); return; }
    RecordLockHandle account_lock;
    if (lock_table_acquire(LOCK_RECORD_ACCOUNT, sess->account_id, &account_lock) != 0) {
        send_frame(fd, OP_VIEW_DETAILS, "ERR|account unavailable"); return;
    }
    rwlock_acquire_read(account_lock.lock);
    Account acc;
    int rc = storage_read_account(sess->account_id, &acc);
    rwlock_release_read(account_lock.lock);
    lock_table_release(&account_lock);

    if (rc != 0) { send_frame(fd, OP_VIEW_DETAILS, "ERR|account unavailable"); return; }
    char details[MAX_PAYLOAD - 8];
    fmt_account_details(&acc, details, sizeof(details));
    char resp[MAX_PAYLOAD];
    snprintf(resp, sizeof(resp), "OK|%s", details);
    send_frame(fd, OP_VIEW_DETAILS, resp);
}

/* ---------- loans ---------- */

void handle_loan_apply(int fd, char *payload, ClientSession *sess, int msqid) {
    if (sess->role != SESSION_USER) { send_frame(fd, OP_LOAN_APPLY, "ERR|not logged in"); return; }
    char *fields[2];
    if (count_char(payload, '|') != 1 || split_fields(payload, fields, 2) != 2) {
        send_frame(fd, OP_LOAN_APPLY, "ERR|malformed request, expected amount|purpose");
        return;
    }
    double amount;
    if (!parse_amount(fields[0], &amount, 0) || amount <= 0) {
        send_frame(fd, OP_LOAN_APPLY, "ERR|invalid amount");
        return;
    }
    if (has_reserved_char(fields[1])) {
        send_frame(fd, OP_LOAN_APPLY, "ERR|purpose must not contain '|' or ';'");
        return;
    }

    Loan ln;
    memset(&ln, 0, sizeof(ln));
    ln.account_id = sess->account_id;
    ln.amount = amount;
    snprintf(ln.purpose, TEXT_LEN, "%s", fields[1]);
    ln.status = LOAN_PENDING;
    if (storage_add_loan(&ln) != 0) { send_frame(fd, OP_LOAN_APPLY, "ERR|could not save application"); return; }

    char log[LOG_MSG_LEN];
    snprintf(log, sizeof(log), "LOAN_APPLY account=%d loan=%d amount=%.2f", sess->account_id, ln.loan_id, amount);
    ipc_send_log(msqid, log);

    char resp[64];
    snprintf(resp, sizeof(resp), "OK|%d", ln.loan_id);
    send_frame(fd, OP_LOAN_APPLY, resp);
}

typedef struct { ListBuilder *lb; int filter_id; } LoanFilterCtx;

static int loan_visit_by_account(const Loan *ln, void *ctx_v) {
    LoanFilterCtx *ctx = ctx_v;
    if (ln->account_id == ctx->filter_id) {
        char entry[320];
        fmt_loan(ln, entry, sizeof(entry));
        list_builder_add(ctx->lb, entry);
    }
    return 1;
}

void handle_loan_my_list(int fd, ClientSession *sess) {
    if (sess->role != SESSION_USER) { send_frame(fd, OP_LOAN_MY_LIST, "ERR|not logged in"); return; }
    ListBuilder lb;
    list_builder_init(&lb, (size_t)MAX_LOANS * 200 + 16);
    if (!lb.buf) { send_frame(fd, OP_LOAN_MY_LIST, "ERR|out of memory"); return; }
    LoanFilterCtx ctx = { &lb, sess->account_id };
    storage_for_each_loan(loan_visit_by_account, &ctx);
    list_builder_respond(fd, OP_LOAN_MY_LIST, &lb);
}

void handle_feedback_add(int fd, char *payload, ClientSession *sess, int msqid) {
    if (sess->role != SESSION_USER) { send_frame(fd, OP_FEEDBACK_ADD, "ERR|not logged in"); return; }
    if (has_reserved_char(payload)) {
        send_frame(fd, OP_FEEDBACK_ADD, "ERR|feedback must not contain '|' or ';'");
        return;
    }
    Feedback fb;
    memset(&fb, 0, sizeof(fb));
    fb.account_id = sess->account_id;
    fb.status = FEEDBACK_ACTIVE;
    snprintf(fb.text, TEXT_LEN, "%s", payload);
    if (storage_add_feedback(&fb) != 0) { send_frame(fd, OP_FEEDBACK_ADD, "ERR|could not save feedback"); return; }

    char log[LOG_MSG_LEN];
    snprintf(log, sizeof(log), "FEEDBACK_ADD account=%d feedback=%d", sess->account_id, fb.feedback_id);
    ipc_send_log(msqid, log);

    char resp[64];
    snprintf(resp, sizeof(resp), "OK|%d", fb.feedback_id);
    send_frame(fd, OP_FEEDBACK_ADD, resp);
}

#define TXN_HISTORY_LIMIT 20

void handle_txn_history(int fd, char *payload, ClientSession *sess) {
    int target_id;
    if (sess->role == SESSION_USER) {
        target_id = sess->account_id;
    } else if (role_is_employee(sess)) {
        target_id = atoi(payload);
        if (target_id <= 0 || target_id > MAX_ACCOUNTS) {
            send_frame(fd, OP_TXN_HISTORY, "ERR|invalid account id");
            return;
        }
    } else {
        send_frame(fd, OP_TXN_HISTORY, "ERR|not logged in");
        return;
    }

    char *history = journal_history_for_account(target_id, TXN_HISTORY_LIMIT);
    if (!history) { send_frame(fd, OP_TXN_HISTORY, "ERR|could not read ledger"); return; }
    size_t len = strlen(history);
    char *resp = malloc(len + 4);
    if (!resp) { free(history); send_frame(fd, OP_TXN_HISTORY, "ERR|out of memory"); return; }
    snprintf(resp, len + 4, "OK|%s", history);
    send_frame(fd, OP_TXN_HISTORY, resp);
    free(resp);
    free(history);
}

int customer_dispatch(int fd, unsigned char opcode, char *payload, ClientSession *session) {
    switch ((Opcode)opcode) {
        case OP_DEPOSIT: handle_deposit(fd, payload, session, 0); return 1;
        case OP_WITHDRAW: handle_withdraw(fd, payload, session, 0); return 1;
        case OP_TRANSFER: handle_transfer(fd, payload, session, 0); return 1;
        case OP_BALANCE: handle_balance(fd, session); return 1;
        case OP_VIEW_DETAILS: handle_view_details(fd, session); return 1;
        case OP_TXN_HISTORY: handle_txn_history(fd, payload, session); return 1;
        case OP_LOAN_APPLY: handle_loan_apply(fd, payload, session, 0); return 1;
        case OP_LOAN_MY_LIST: handle_loan_my_list(fd, session); return 1;
        case OP_FEEDBACK_ADD: handle_feedback_add(fd, payload, session, 0); return 1;
        default: return 0;
    }
}

