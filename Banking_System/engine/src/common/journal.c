#include "journal.h"
#include "storage.h"

#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>

#define WAL_MAGIC 0x42414e4b57414c31ULL /* "BANKWAL1" */
#define LOAN_WAL_MAGIC 0x42414e4b4c4e5731ULL /* "BANKLNW1" */
#define WAL_PREPARED 1
#define WAL_COMMITTED 2
#define WAL_ABORTED 3

typedef struct {
    unsigned long long magic;
    unsigned long long transaction_id;
    unsigned int state;
    unsigned int account_count;
    unsigned long long ledger_offset_a;
    unsigned long long ledger_offset_b;
    Account before_a, after_a;
    Account before_b, after_b;
    LedgerEntry entry_a, entry_b;
    unsigned long long checksum;
} WalRecord;

typedef struct {
    unsigned long long magic;
    unsigned long long transaction_id;
    unsigned int state;
    unsigned int reserved;
    unsigned long long ledger_offset;
    Account before_account, after_account;
    Loan before_loan, after_loan;
    LedgerEntry entry;
    unsigned long long checksum;
} LoanWalRecord;

static int g_wal_fd = -1;
static int g_loan_wal_fd = -1;
static int g_ledger_fd = -1;
static unsigned long long g_next_transaction_id = 1;
static pthread_mutex_t g_wal_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_ledger_mutex = PTHREAD_MUTEX_INITIALIZER;
/* A history query takes the write side so it cannot observe a ledger entry
 * between PREPARE and durable COMMIT. Financial commits take the read side,
 * allowing unrelated money operations to continue concurrently. */
static pthread_rwlock_t g_ledger_visibility_gate = PTHREAD_RWLOCK_INITIALIZER;
/* Financial workers hold a read lease for their entire transaction. The
 * periodic recovery worker takes the write side, so it can never mistake a
 * still-running PREPARED transaction for a crashed one. */
static pthread_rwlock_t g_recovery_gate = PTHREAD_RWLOCK_INITIALIZER;
static volatile int g_recovery_worker_running = 0;
static unsigned int g_recovery_interval = 60;

static unsigned long long checksum(const WalRecord *record) {
    const unsigned char *p = (const unsigned char *)record;
    size_t n = offsetof(WalRecord, checksum);
    unsigned long long h = 1469598103934665603ULL;
    for (size_t i = 0; i < n; ++i) h = (h ^ p[i]) * 1099511628211ULL;
    return h;
}

static unsigned long long loan_checksum(const LoanWalRecord *record) {
    const unsigned char *p = (const unsigned char *)record;
    size_t n = offsetof(LoanWalRecord, checksum);
    unsigned long long h = 1469598103934665603ULL;
    for (size_t i = 0; i < n; ++i) h = (h ^ p[i]) * 1099511628211ULL;
    return h;
}

static int write_all_at(int fd, const void *buf, size_t size, off_t offset) {
    const char *p = buf;
    size_t done = 0;
    while (done < size) {
        ssize_t n = pwrite(fd, p + done, size - done, offset + (off_t)done);
        if (n <= 0) return -1;
        done += (size_t)n;
    }
    return 0;
}

static int read_all_at(int fd, void *buf, size_t size, off_t offset) {
    char *p = buf;
    size_t done = 0;
    while (done < size) {
        ssize_t n = pread(fd, p + done, size - done, offset + (off_t)done);
        if (n <= 0) return -1;
        done += (size_t)n;
    }
    return 0;
}

static int sync_fd(int fd) { return fdatasync(fd) == 0 ? 0 : -1; }

static int write_ledger_at(unsigned long long offset, const LedgerEntry *entry) {
    return write_all_at(g_ledger_fd, entry, sizeof(*entry), (off_t)offset) == 0 && sync_fd(g_ledger_fd) == 0 ? 0 : -1;
}

static int clear_ledger_at(unsigned long long offset) {
    LedgerEntry empty;
    memset(&empty, 0, sizeof(empty));
    return write_ledger_at(offset, &empty);
}

static int reserve_ledger_slots(int count, unsigned long long *first) {
    pthread_mutex_lock(&g_ledger_mutex);
    struct stat st;
    int ok = fstat(g_ledger_fd, &st) == 0;
    unsigned long long offset = ok ? (unsigned long long)st.st_size : 0;
    if (ok && ftruncate(g_ledger_fd, st.st_size + (off_t)count * (off_t)sizeof(LedgerEntry)) == 0 && sync_fd(g_ledger_fd) == 0) {
        *first = offset;
    } else {
        ok = 0;
    }
    pthread_mutex_unlock(&g_ledger_mutex);
    return ok ? 0 : -1;
}

static int persist_wal(off_t offset, WalRecord *record) {
    record->checksum = checksum(record);
    return write_all_at(g_wal_fd, record, sizeof(*record), offset) == 0 && sync_fd(g_wal_fd) == 0 ? 0 : -1;
}

static int apply_record(const WalRecord *record, int redo) {
    const Account *a = redo ? &record->after_a : &record->before_a;
    const Account *b = redo ? &record->after_b : &record->before_b;
    if (storage_write_account(a) != 0) return -1;
    if (record->account_count == 2 && storage_write_account(b) != 0) return -1;
    if (redo) {
        if (write_ledger_at(record->ledger_offset_a, &record->entry_a) != 0) return -1;
        if (record->account_count == 2 && write_ledger_at(record->ledger_offset_b, &record->entry_b) != 0) return -1;
    } else {
        if (clear_ledger_at(record->ledger_offset_a) != 0) return -1;
        if (record->account_count == 2 && clear_ledger_at(record->ledger_offset_b) != 0) return -1;
    }
    return 0;
}

static int apply_loan_record(const LoanWalRecord *record, int redo) {
    if (storage_write_account(redo ? &record->after_account : &record->before_account) != 0) return -1;
    if (storage_update_loan(redo ? &record->after_loan : &record->before_loan) != 0) return -1;
    if (redo) return write_ledger_at(record->ledger_offset, &record->entry);
    return clear_ledger_at(record->ledger_offset);
}

static int persist_loan_wal(off_t offset, LoanWalRecord *record) {
    record->checksum = loan_checksum(record);
    return write_all_at(g_loan_wal_fd, record, sizeof(*record), offset) == 0 &&
           sync_fd(g_loan_wal_fd) == 0 ? 0 : -1;
}

static int recover_locked(int redo_committed) {
    struct stat st;
    if (fstat(g_wal_fd, &st) != 0) return -1;
    off_t full_bytes = st.st_size - (st.st_size % (off_t)sizeof(WalRecord));
    if (ftruncate(g_wal_fd, full_bytes) != 0 || sync_fd(g_wal_fd) != 0) return -1;
    for (off_t offset = 0; offset < full_bytes; offset += (off_t)sizeof(WalRecord)) {
        WalRecord record;
        if (read_all_at(g_wal_fd, &record, sizeof(record), offset) != 0) return -1;
        if (record.magic != WAL_MAGIC || record.checksum != checksum(&record)) {
            /* A torn final WAL append has no durable PREPARE guarantee and
             * therefore no data writes may have followed it. Discard only
             * that tail; corruption in an older record is fail-closed. */
            if (offset + (off_t)sizeof(WalRecord) == full_bytes) {
                if (ftruncate(g_wal_fd, offset) != 0 || sync_fd(g_wal_fd) != 0) return -1;
                break;
            }
            return -1;
        }
        if (record.transaction_id >= g_next_transaction_id) g_next_transaction_id = record.transaction_id + 1;
        if (record.state == WAL_COMMITTED && redo_committed) {
            if (apply_record(&record, 1) != 0) return -1;
        } else if (record.state == WAL_PREPARED) {
            if (apply_record(&record, 0) != 0) return -1;
            record.state = WAL_ABORTED;
            if (persist_wal(offset, &record) != 0) return -1;
        }
    }
    return 0;
}

static int recover_loan_locked(int redo_committed) {
    struct stat st;
    if (fstat(g_loan_wal_fd, &st) != 0) return -1;
    off_t full_bytes = st.st_size - (st.st_size % (off_t)sizeof(LoanWalRecord));
    if (ftruncate(g_loan_wal_fd, full_bytes) != 0 || sync_fd(g_loan_wal_fd) != 0) return -1;
    for (off_t offset = 0; offset < full_bytes; offset += (off_t)sizeof(LoanWalRecord)) {
        LoanWalRecord record;
        if (read_all_at(g_loan_wal_fd, &record, sizeof(record), offset) != 0) return -1;
        if (record.magic != LOAN_WAL_MAGIC || record.checksum != loan_checksum(&record)) {
            if (offset + (off_t)sizeof(LoanWalRecord) == full_bytes) {
                if (ftruncate(g_loan_wal_fd, offset) != 0 || sync_fd(g_loan_wal_fd) != 0) return -1;
                break;
            }
            return -1;
        }
        if (record.transaction_id >= g_next_transaction_id) g_next_transaction_id = record.transaction_id + 1;
        if (record.state == WAL_COMMITTED && redo_committed) {
            if (apply_loan_record(&record, 1) != 0) return -1;
        } else if (record.state == WAL_PREPARED) {
            if (apply_loan_record(&record, 0) != 0) return -1;
            record.state = WAL_ABORTED;
            if (persist_loan_wal(offset, &record) != 0) return -1;
        }
    }
    return 0;
}

int journal_init_and_recover(void) {
    g_wal_fd = open(WAL_FILE, O_RDWR | O_CREAT, 0600);
    g_loan_wal_fd = open(LOAN_WAL_FILE, O_RDWR | O_CREAT, 0600);
    g_ledger_fd = open(LEDGER_FILE, O_RDWR | O_CREAT, 0600);
    if (g_wal_fd < 0 || g_loan_wal_fd < 0 || g_ledger_fd < 0) return -1;
    if (recover_locked(1) != 0) return -1;
    return recover_loan_locked(1);
}

static void *recovery_worker(void *unused) {
    (void)unused;
    while (g_recovery_worker_running) {
        sleep(g_recovery_interval);
        if (!g_recovery_worker_running) break;
        /* Quiesce money-changing workers first. In a healthy live process
         * this is mostly a cheap integrity sweep; after a thread-level
         * failure it repairs any PREPARED-only record it can safely see. */
        pthread_rwlock_wrlock(&g_recovery_gate);
        (void)recover_locked(0);
        (void)recover_loan_locked(0);
        pthread_rwlock_unlock(&g_recovery_gate);
    }
    return NULL;
}

int journal_start_recovery_worker(unsigned int interval_seconds) {
    pthread_t thread;
    g_recovery_interval = interval_seconds ? interval_seconds : 60;
    g_recovery_worker_running = 1;
    if (pthread_create(&thread, NULL, recovery_worker, NULL) != 0) {
        g_recovery_worker_running = 0;
        return -1;
    }
    pthread_detach(thread);
    return 0;
}

void journal_close(void) {
    g_recovery_worker_running = 0;
    if (g_wal_fd >= 0) close(g_wal_fd);
    if (g_loan_wal_fd >= 0) close(g_loan_wal_fd);
    if (g_ledger_fd >= 0) close(g_ledger_fd);
    g_wal_fd = g_loan_wal_fd = g_ledger_fd = -1;
}

int journal_commit_financial(const Account *before_a, const Account *after_a,
                             const Account *before_b, const Account *after_b,
                             LedgerEntry *entry_a, LedgerEntry *entry_b) {
    if (g_wal_fd < 0 || g_ledger_fd < 0 || !before_a || !after_a || !entry_a) return -1;
    pthread_rwlock_rdlock(&g_recovery_gate);
    pthread_rwlock_rdlock(&g_ledger_visibility_gate);
    int count = before_b && after_b && entry_b ? 2 : 1;
    unsigned long long ledger_offset;
    if (reserve_ledger_slots(count, &ledger_offset) != 0) {
        pthread_rwlock_unlock(&g_ledger_visibility_gate);
        pthread_rwlock_unlock(&g_recovery_gate);
        return -1;
    }

    WalRecord record;
    memset(&record, 0, sizeof(record));
    record.magic = WAL_MAGIC;
    record.state = WAL_PREPARED;
    record.account_count = (unsigned int)count;
    record.ledger_offset_a = ledger_offset;
    record.ledger_offset_b = ledger_offset + sizeof(LedgerEntry);
    record.before_a = *before_a; record.after_a = *after_a;
    if (count == 2) { record.before_b = *before_b; record.after_b = *after_b; }

    pthread_mutex_lock(&g_wal_mutex);
    record.transaction_id = g_next_transaction_id++;
    entry_a->transaction_id = record.transaction_id;
    if (count == 2) entry_b->transaction_id = record.transaction_id;
    record.entry_a = *entry_a;
    if (count == 2) record.entry_b = *entry_b;
    off_t wal_offset = lseek(g_wal_fd, 0, SEEK_END);
    int prepared = wal_offset >= 0 && persist_wal(wal_offset, &record) == 0;
    pthread_mutex_unlock(&g_wal_mutex);
    if (!prepared) {
        pthread_rwlock_unlock(&g_ledger_visibility_gate);
        pthread_rwlock_unlock(&g_recovery_gate);
        return -1;
    }

    if (apply_record(&record, 1) != 0) {
        (void)apply_record(&record, 0);
        record.state = WAL_ABORTED;
        pthread_mutex_lock(&g_wal_mutex);
        (void)persist_wal(wal_offset, &record);
        pthread_mutex_unlock(&g_wal_mutex);
        pthread_rwlock_unlock(&g_ledger_visibility_gate);
        pthread_rwlock_unlock(&g_recovery_gate);
        return -1;
    }

    record.state = WAL_COMMITTED;
    pthread_mutex_lock(&g_wal_mutex);
    /* persist_wal(), like the storage layer, returns 0 on success. Keep
     * that convention all the way back to financial handlers: only a
     * nonzero result means the COMMIT record was not made durable. */
    int commit_rc = persist_wal(wal_offset, &record);
    pthread_mutex_unlock(&g_wal_mutex);
    if (commit_rc != 0) {
        /* Do not expose a ledger entry for a transaction whose COMMIT record
         * could not be made durable. A later recovery sweep will retry the
         * undo if the best-effort rollback itself encounters an I/O error. */
        (void)apply_record(&record, 0);
        record.state = WAL_ABORTED;
        pthread_mutex_lock(&g_wal_mutex);
        (void)persist_wal(wal_offset, &record);
        pthread_mutex_unlock(&g_wal_mutex);
    }
    pthread_rwlock_unlock(&g_ledger_visibility_gate);
    pthread_rwlock_unlock(&g_recovery_gate);
    return commit_rc;
}

int journal_approve_loan(const Account *before, const Account *after,
                         const Loan *loan_before, const Loan *loan_after,
                         LedgerEntry *entry) {
    if (g_loan_wal_fd < 0 || g_ledger_fd < 0 || !before || !after ||
        !loan_before || !loan_after || !entry ||
        loan_before->status != LOAN_PENDING || loan_after->status != LOAN_APPROVED ||
        loan_before->loan_id != loan_after->loan_id ||
        before->account_id != after->account_id ||
        before->account_id != loan_before->account_id) return -1;

    pthread_rwlock_rdlock(&g_recovery_gate);
    pthread_rwlock_rdlock(&g_ledger_visibility_gate);
    unsigned long long ledger_offset;
    if (reserve_ledger_slots(1, &ledger_offset) != 0) {
        pthread_rwlock_unlock(&g_ledger_visibility_gate);
        pthread_rwlock_unlock(&g_recovery_gate);
        return -1;
    }

    LoanWalRecord record;
    memset(&record, 0, sizeof(record));
    record.magic = LOAN_WAL_MAGIC;
    record.state = WAL_PREPARED;
    record.ledger_offset = ledger_offset;
    record.before_account = *before;
    record.after_account = *after;
    record.before_loan = *loan_before;
    record.after_loan = *loan_after;

    pthread_mutex_lock(&g_wal_mutex);
    record.transaction_id = g_next_transaction_id++;
    entry->transaction_id = record.transaction_id;
    record.entry = *entry;
    off_t wal_offset = lseek(g_loan_wal_fd, 0, SEEK_END);
    int prepared = wal_offset >= 0 && persist_loan_wal(wal_offset, &record) == 0;
    pthread_mutex_unlock(&g_wal_mutex);
    if (!prepared) {
        pthread_rwlock_unlock(&g_ledger_visibility_gate);
        pthread_rwlock_unlock(&g_recovery_gate);
        return -1;
    }

    if (apply_loan_record(&record, 1) != 0) {
        (void)apply_loan_record(&record, 0);
        record.state = WAL_ABORTED;
        pthread_mutex_lock(&g_wal_mutex);
        (void)persist_loan_wal(wal_offset, &record);
        pthread_mutex_unlock(&g_wal_mutex);
        pthread_rwlock_unlock(&g_ledger_visibility_gate);
        pthread_rwlock_unlock(&g_recovery_gate);
        return -1;
    }

    record.state = WAL_COMMITTED;
    pthread_mutex_lock(&g_wal_mutex);
    int commit_rc = persist_loan_wal(wal_offset, &record);
    pthread_mutex_unlock(&g_wal_mutex);
    if (commit_rc != 0) {
        (void)apply_loan_record(&record, 0);
        record.state = WAL_ABORTED;
        pthread_mutex_lock(&g_wal_mutex);
        (void)persist_loan_wal(wal_offset, &record);
        pthread_mutex_unlock(&g_wal_mutex);
    }
    pthread_rwlock_unlock(&g_ledger_visibility_gate);
    pthread_rwlock_unlock(&g_recovery_gate);
    return commit_rc;
}

char *journal_history_for_account(int account_id, int limit) {
    if (account_id <= 0 || limit <= 0) return NULL;
    pthread_rwlock_rdlock(&g_recovery_gate);
    pthread_rwlock_wrlock(&g_ledger_visibility_gate);
    struct stat st;
    if (g_ledger_fd < 0 || fstat(g_ledger_fd, &st) != 0) {
        pthread_rwlock_unlock(&g_ledger_visibility_gate);
        pthread_rwlock_unlock(&g_recovery_gate);
        return NULL;
    }
    size_t slots = (size_t)(st.st_size / (off_t)sizeof(LedgerEntry));
    size_t cap = (size_t)limit * 256 + 16, pos = 0;
    char *out = calloc(1, cap);
    if (!out) {
        pthread_rwlock_unlock(&g_ledger_visibility_gate);
        pthread_rwlock_unlock(&g_recovery_gate);
        return NULL;
    }
    int returned = 0;
    for (size_t i = slots; i > 0 && returned < limit; --i) {
        LedgerEntry entry;
        if (read_all_at(g_ledger_fd, &entry, sizeof(entry), (off_t)(i - 1) * (off_t)sizeof(entry)) != 0) continue;
        if (entry.transaction_id == 0 || entry.account_id != account_id) continue;
        int n = snprintf(out + pos, cap - pos, "%s%s|%d|%s|%s|%.2f|%.2f|%s",
                         pos ? ";" : "", entry.timestamp, entry.account_id, entry.owner,
                         entry.operation, entry.amount, entry.balance_after,
                         entry.counterparty_account_id ? "" : "-");
        if (entry.counterparty_account_id) n = snprintf(out + pos, cap - pos, "%s%s|%d|%s|%s|%.2f|%.2f|%d",
                         pos ? ";" : "", entry.timestamp, entry.account_id, entry.owner,
                         entry.operation, entry.amount, entry.balance_after, entry.counterparty_account_id);
        if (n < 0 || (size_t)n >= cap - pos) break;
        pos += (size_t)n;
        returned++;
    }
    pthread_rwlock_unlock(&g_ledger_visibility_gate);
    pthread_rwlock_unlock(&g_recovery_gate);
    return out;
}
