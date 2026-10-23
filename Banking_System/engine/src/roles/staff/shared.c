#include "common.h"
#include "protocol.h"
#include "storage.h"
#include "lock_table.h"
#include "session.h"
#include "role_access.h"
#include "response_format.h"

#include <stdio.h>
#include <stdlib.h>

/* These views are intentionally shared by Employee and Manager. Keeping them
 * here prevents two copies of the record-locking and response-format logic. */
void handle_admin_search(int fd, char *payload, ClientSession *sess) {
    if (!role_is_staff_any(sess)) { send_frame(fd, OP_ADMIN_SEARCH, "ERR|not authorized"); return; }
    int id = atoi(payload);
    RecordLockHandle account_lock;
    if (lock_table_acquire(LOCK_RECORD_ACCOUNT, id, &account_lock) != 0) {
        send_frame(fd, OP_ADMIN_SEARCH, "ERR|invalid account id"); return;
    }
    rwlock_acquire_read(account_lock.lock);
    Account acc;
    int rc = storage_read_account(id, &acc);
    rwlock_release_read(account_lock.lock);
    lock_table_release(&account_lock);
    if (rc != 0 || acc.status == STATUS_EMPTY) { send_frame(fd, OP_ADMIN_SEARCH, "ERR|account not found"); return; }
    char details[MAX_PAYLOAD - 8];
    fmt_account_details(&acc, details, sizeof(details));
    char resp[MAX_PAYLOAD];
    snprintf(resp, sizeof(resp), "OK|%s", details);
    send_frame(fd, OP_ADMIN_SEARCH, resp);
}

void handle_admin_list(int fd, ClientSession *sess) {
    if (!role_is_staff_any(sess)) { send_frame(fd, OP_ADMIN_LIST, "ERR|not authorized"); return; }
    ListBuilder lb;
    list_builder_init(&lb, (size_t)MAX_ACCOUNTS * 160 + 16);
    if (!lb.buf) { send_frame(fd, OP_ADMIN_LIST, "ERR|out of memory"); return; }
    for (int id = 1; id <= MAX_ACCOUNTS; id++) {
        RecordLockHandle account_lock;
        if (lock_table_acquire(LOCK_RECORD_ACCOUNT, id, &account_lock) != 0) continue;
        rwlock_acquire_read(account_lock.lock);
        Account acc;
        int rc = storage_read_account(id, &acc);
        rwlock_release_read(account_lock.lock);
        lock_table_release(&account_lock);
        if (rc != 0 || acc.status == STATUS_EMPTY || acc.status == STATUS_DELETED) continue;
        char details[200];
        fmt_account_details(&acc, details, sizeof(details));
        list_builder_add(&lb, details);
    }
    list_builder_respond(fd, OP_ADMIN_LIST, &lb);
}

int staff_shared_dispatch(int fd, unsigned char opcode, char *payload, ClientSession *session) {
    switch ((Opcode)opcode) {
        case OP_ADMIN_SEARCH: handle_admin_search(fd, payload, session); return 1;
        case OP_ADMIN_LIST: handle_admin_list(fd, session); return 1;
        default: return 0;
    }
}
