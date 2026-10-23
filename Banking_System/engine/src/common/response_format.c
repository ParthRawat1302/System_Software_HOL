#include "common.h"
#include "protocol.h"
#include "response_format.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
void fmt_timestamp(char *out, size_t out_size) {
    time_t now = time(NULL);
    struct tm tm_buf;
    localtime_r(&now, &tm_buf);
    strftime(out, out_size, "%Y-%m-%d %H:%M:%S", &tm_buf);
}

void fmt_account_details(const Account *acc, char *out, size_t out_size) {
    const char *role_str = acc->role == ROLE_JOINT ? "JOINT" : "NORMAL";
    const char *status_str = acc->status == STATUS_ACTIVE ? "ACTIVE"
                              : acc->status == STATUS_INACTIVE ? "INACTIVE" : "DELETED";
    if (acc->role == ROLE_JOINT) {
        snprintf(out, out_size, "id=%d|role=%s|status=%s|owner1=%s|owner2=%s|balance=%.2f",
                 acc->account_id, role_str, status_str, acc->owner1_name, acc->owner2_name, acc->balance);
    } else {
        snprintf(out, out_size, "id=%d|role=%s|status=%s|owner1=%s|balance=%.2f",
                 acc->account_id, role_str, status_str, acc->owner1_name, acc->balance);
    }
}

void fmt_staff(const Staff *st, char *out, size_t out_size) {
    const char *role_str = st->role == STAFF_MANAGER ? "MANAGER" : "EMPLOYEE";
    const char *status_str = st->status == STATUS_ACTIVE ? "ACTIVE" : "DELETED";
    snprintf(out, out_size, "id=%d|role=%s|status=%s|name=%s", st->staff_id, role_str, status_str, st->name);
}

void fmt_loan(const Loan *ln, char *out, size_t out_size) {
    const char *status_str = ln->status == LOAN_PENDING ? "PENDING"
                              : ln->status == LOAN_APPROVED ? "APPROVED"
                              : ln->status == LOAN_REJECTED ? "REJECTED" : "UNKNOWN";
    snprintf(out, out_size, "id=%d|account=%d|amount=%.2f|purpose=%s|status=%s|assigned=%d",
             ln->loan_id, ln->account_id, ln->amount, ln->purpose, status_str, ln->assigned_employee_id);
}

void fmt_loan_history(const Loan *ln, char *out, size_t out_size) {
    const char *status_str = ln->status == LOAN_PENDING ? "PENDING"
                              : ln->status == LOAN_APPROVED ? "APPROVED"
                              : ln->status == LOAN_REJECTED ? "REJECTED" : "UNKNOWN";
    snprintf(out, out_size,
             "id=%d|account=%d|amount=%.2f|purpose=%s|status=%s|assigned=%d|decided_by=%d",
             ln->loan_id, ln->account_id, ln->amount, ln->purpose, status_str,
             ln->assigned_employee_id, ln->decided_by);
}

/* Growable ';'-joined list builder, used by every "list many records"
 * handler (admin/staff/loan listings) -- one small helper instead of
 * repeating the same malloc/snprintf/grow dance four times. */
void list_builder_init(ListBuilder *lb, size_t cap) {
    lb->buf = malloc(cap);
    lb->cap = cap;
    lb->pos = 0;
    lb->first = 1;
    if (lb->buf) lb->buf[0] = '\0';
}

void list_builder_add(ListBuilder *lb, const char *entry) {
    if (!lb->buf) return;
    int n = snprintf(lb->buf + lb->pos, lb->cap - lb->pos, "%s%s", lb->first ? "" : ";", entry);
    if (n < 0) return;
    size_t remaining = lb->cap - lb->pos;
    lb->pos += ((size_t)n < remaining) ? (size_t)n : (remaining - 1);
    lb->first = 0;
}

void list_builder_respond(int fd, unsigned char opcode, ListBuilder *lb) {
    if (!lb->buf) { send_frame(fd, opcode, "ERR|out of memory"); return; }
    char *resp = malloc(lb->pos + 4);
    if (!resp) { send_frame(fd, opcode, "ERR|out of memory"); free(lb->buf); return; }
    snprintf(resp, lb->pos + 4, "OK|%s", lb->buf);
    send_frame(fd, opcode, resp);
    free(resp);
    free(lb->buf);
}


