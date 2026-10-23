#ifndef STORAGE_H
#define STORAGE_H

#include "common.h"

/* Creates the data directory/files if missing and seeds default customer,
 * staff, and admin records into empty ID-1 slots without replacing existing
 * records. Returns 0 on success. */
int storage_init(void);

/* Raw record I/O for accounts/admins. All of these use open()/pread()/
 * pwrite() directly -- no stdio, no database library. Every write is
 * followed by fdatasync() so a committed transaction survives a crash
 * (the "D" in ACID) -- see docs/design.md. Callers are responsible for
 * holding the matching dynamic record lock (see lock_table.h) around
 * read-modify-write sequences. */
int storage_read_account(int account_id, Account *out);   /* 0 ok, -1 not found */
int storage_write_account(const Account *acc);              /* 0 ok, -1 error */
int storage_find_free_slot(void);                             /* returns id, or -1 if full */

int storage_read_admin(int admin_id, Admin *out);
int storage_write_admin(const Admin *adm);

/* Administrators, staff, and loans use the dynamic per-record registry at their
 * call sites. storage_add_staff() retains only a small allocation mutex so
 * its find-a-free-slot-and-claim operation remains atomic. */
int storage_add_staff(Staff *st);                    /* fills st->staff_id, returns 0/-1 */
int storage_read_staff(StaffRole role, int staff_id, Staff *out);
int storage_update_staff(StaffRole role, const Staff *st);
int storage_staff_lock_id(StaffRole role, int staff_id);
int storage_change_staff_role(StaffRole from_role, int staff_id, StaffRole to_role);

int storage_add_loan(Loan *ln);                       /* fills ln->loan_id, returns 0/-1 */
int storage_read_loan(int loan_id, Loan *out);
int storage_update_loan(const Loan *ln);
int storage_for_each_loan(int (*visit)(const Loan *ln, void *ctx), void *ctx);

int storage_add_feedback(Feedback *fb);               /* fills fb->feedback_id, returns 0/-1 */
int storage_read_feedback(int feedback_id, Feedback *out);
int storage_update_feedback(const Feedback *fb);
int storage_for_each_feedback(int (*visit)(const Feedback *fb, void *ctx), void *ctx);

#endif
