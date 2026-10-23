#ifndef JOURNAL_H
#define JOURNAL_H

#include "common.h"

/* Opens the durable write-ahead log, repairs interrupted financial
 * transactions, then makes the journal available to worker threads. */
int journal_init_and_recover(void);
/* Starts a safe periodic recovery sweep. It waits for all active financial
 * transactions before inspecting PREPARED records. */
int journal_start_recovery_worker(unsigned int interval_seconds);
void journal_close(void);

/* Atomically commits one or two account images and their corresponding
 * passbook entries.  The caller must hold every affected account write lock.
 * Returns 0 only after WAL, account records, ledger records, and COMMIT are
 * all durable. */
int journal_commit_financial(const Account *before_a, const Account *after_a,
                             const Account *before_b, const Account *after_b,
                             LedgerEntry *entry_a, LedgerEntry *entry_b);

/* Atomically records an approved loan, credits its account, and appends the
 * disbursement to the durable ledger under the same WAL transaction. */
int journal_approve_loan(const Account *before, const Account *after,
                         const Loan *loan_before, const Loan *loan_after,
                         LedgerEntry *entry);

/* Reads the newest ledger entries for one account.  The returned wire-format
 * string is owned by the caller. */
char *journal_history_for_account(int account_id, int limit);

#endif
