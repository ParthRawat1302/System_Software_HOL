#include "storage.h"
#include "util.h"
#include "lock_table.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

static int g_accounts_fd = -1;
static int g_admins_fd = -1;
static int g_staff_fd = -1;
static int g_loans_fd = -1;
static int g_feedback_fd = -1;

/* Account, administrator, and staff record locks live in lock_table.c.
 * This mutex only protects staff ID allocation's scan-and-claim sequence. */
static pthread_mutex_t g_staff_creation_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_loan_creation_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_feedback_mutex = PTHREAD_MUTEX_INITIALIZER;
static int migrate_legacy_staff_records(void);

static int open_data_file(const char *path) {
    int fd = open(path, O_RDWR | O_CREAT, 0600);
    return fd;
}

int storage_init(void) {
    if (mkdir(DATA_DIR, 0700) != 0 && errno != EEXIST) {
        perror("mkdir data");
        return -1;
    }

    g_accounts_fd = open_data_file(ACCOUNTS_FILE);
    g_admins_fd = open_data_file(ADMINS_FILE);
    g_staff_fd = open_data_file(STAFF_FILE);
    g_loans_fd = open_data_file(LOANS_FILE);
    g_feedback_fd = open_data_file(FEEDBACK_FILE);
    if (g_accounts_fd < 0 || g_admins_fd < 0 || g_staff_fd < 0 || g_loans_fd < 0 || g_feedback_fd < 0) {
        perror("open data files");
        return -1;
    }
    if (migrate_legacy_staff_records() != 0) {
        fprintf(stderr, "migrate legacy staff records failed\n");
        return -1;
    }

    /* Seed missing demo identities without replacing existing records. */
    Account user;
    if (storage_read_account(1, &user) != 0) {
        fprintf(stderr, "read default user slot failed\n");
        return -1;
    }
    if (user.status == STATUS_EMPTY) {
        memset(&user, 0, sizeof(user));
        user.account_id = 1;
        user.role = ROLE_NORMAL;
        user.status = STATUS_ACTIVE;
        snprintf(user.owner1_name, NAME_LEN, "user");
        user.owner1_pw_hash = simple_hash("pass123");
        if (storage_write_account(&user) != 0) {
            fprintf(stderr, "write default user failed\n");
            return -1;
        }
        printf("[storage] seeded USER id=1 name=user password=pass123\n");
    }

    Staff staff;
    if (storage_read_staff(STAFF_EMPLOYEE, 1, &staff) != 0) {
        fprintf(stderr, "read default staff slot failed\n");
        return -1;
    }
    if (staff.status == STATUS_EMPTY) {
        memset(&staff, 0, sizeof(staff));
        staff.staff_id = 1;
        staff.role = STAFF_EMPLOYEE;
        staff.status = STATUS_ACTIVE;
        snprintf(staff.name, NAME_LEN, "staff");
        staff.pw_hash = simple_hash("pass123");
        if (storage_update_staff(STAFF_EMPLOYEE, &staff) != 0) {
            fprintf(stderr, "write default staff failed\n");
            return -1;
        }
        printf("[storage] seeded STAFF id=1 name=staff password=pass123 role=EMPLOYEE\n");
    }

    /* Employee and Manager IDs are separate namespaces. */
    Staff manager;
    if (storage_read_staff(STAFF_MANAGER, 1, &manager) != 0) {
        fprintf(stderr, "read default manager slot failed\n");
        return -1;
    }
    if (manager.status == STATUS_EMPTY) {
        memset(&manager, 0, sizeof(manager));
        manager.staff_id = 1;
        manager.role = STAFF_MANAGER;
        manager.status = STATUS_ACTIVE;
        snprintf(manager.name, NAME_LEN, "manager");
        manager.pw_hash = simple_hash("pass123");
        if (storage_update_staff(STAFF_MANAGER, &manager) != 0) {
            fprintf(stderr, "write default manager failed\n");
            return -1;
        }
        printf("[storage] seeded MANAGER id=1 name=manager password=pass123 role=MANAGER\n");
    }

    Admin existing_admin;
    if (storage_read_admin(1, &existing_admin) != 0) {
        fprintf(stderr, "read default admin slot failed\n");
        return -1;
    }
    if (existing_admin.status == STATUS_EMPTY) {
        Admin adm;
        memset(&adm, 0, sizeof(adm));
        adm.admin_id = 1;
        adm.status = STATUS_ACTIVE;
        snprintf(adm.name, NAME_LEN, "admin");
        adm.pw_hash = simple_hash("pass123");
        if (storage_write_admin(&adm) != 0) {
            fprintf(stderr, "write default admin failed\n");
            return -1;
        }
        printf("[storage] seeded ADMIN id=1 name=admin password=pass123\n");
    }

    return 0;
}

int storage_read_account(int account_id, Account *out) {
    if (account_id <= 0 || account_id > MAX_ACCOUNTS) return -1;
    off_t offset = (off_t)account_id * (off_t)sizeof(Account);
    ssize_t n = pread(g_accounts_fd, out, sizeof(Account), offset);
    if (n == 0) {
        memset(out, 0, sizeof(Account));
        out->account_id = account_id;
        out->status = STATUS_EMPTY;
        return 0;
    }
    if (n != (ssize_t)sizeof(Account)) return -1;
    return 0;
}

int storage_write_account(const Account *acc) {
    if (acc->account_id <= 0 || acc->account_id > MAX_ACCOUNTS) return -1;
    off_t offset = (off_t)acc->account_id * (off_t)sizeof(Account);
    ssize_t n = pwrite(g_accounts_fd, acc, sizeof(Account), offset);
    if (n != (ssize_t)sizeof(Account)) return -1;
    /* Durability (the "D" in ACID): make sure the write actually reaches
     * disk before this call returns, so a committed deposit/withdraw/
     * transfer survives a crash immediately after. */
    return fdatasync(g_accounts_fd) == 0 ? 0 : -1;
}

int storage_find_free_slot(void) {
    for (int id = 1; id <= MAX_ACCOUNTS; id++) {
        Account acc;
        if (storage_read_account(id, &acc) != 0) return -1;
        if (acc.status == STATUS_EMPTY || acc.status == STATUS_DELETED) return id;
    }
    return -1;
}

int storage_read_admin(int admin_id, Admin *out) {
    if (admin_id <= 0 || admin_id > MAX_ADMINS) return -1;
    off_t offset = (off_t)admin_id * (off_t)sizeof(Admin);
    ssize_t n = pread(g_admins_fd, out, sizeof(Admin), offset);
    if (n == 0) {
        memset(out, 0, sizeof(Admin));
        out->admin_id = admin_id;
        out->status = STATUS_EMPTY;
        return 0;
    }
    if (n != (ssize_t)sizeof(Admin)) return -1;
    return 0;
}

int storage_write_admin(const Admin *adm) {
    if (adm->admin_id <= 0 || adm->admin_id > MAX_ADMINS) return -1;
    off_t offset = (off_t)adm->admin_id * (off_t)sizeof(Admin);
    ssize_t n = pwrite(g_admins_fd, adm, sizeof(Admin), offset);
    if (n != (ssize_t)sizeof(Admin)) return -1;
    return fdatasync(g_admins_fd) == 0 ? 0 : -1;
}

/* ---------- staff (Employee / Manager) ---------- */

int storage_staff_lock_id(StaffRole role, int staff_id) {
    if (staff_id <= 0 || staff_id > MAX_STAFF) return -1;
    if (role == STAFF_EMPLOYEE) return staff_id;
    if (role == STAFF_MANAGER) return MAX_STAFF + staff_id;
    return -1;
}

static off_t staff_offset(StaffRole role, int staff_id) {
    int lock_id = storage_staff_lock_id(role, staff_id);
    return lock_id < 0 ? (off_t)-1 : (off_t)lock_id * (off_t)sizeof(Staff);
}

static int staff_read_locked(StaffRole role, int staff_id, Staff *out) {
    if (staff_id <= 0 || staff_id > MAX_STAFF) return -1;
    off_t offset = staff_offset(role, staff_id);
    if (offset < 0) return -1;
    ssize_t n = pread(g_staff_fd, out, sizeof(Staff), offset);
    if (n == 0) {
        memset(out, 0, sizeof(Staff));
        out->staff_id = staff_id;
        out->status = STATUS_EMPTY;
        return 0;
    }
    if (n != (ssize_t)sizeof(Staff)) return -1;
    if (out->status == STATUS_EMPTY && out->role == STAFF_EMPTY) {
        out->staff_id = staff_id;
        return 0;
    }
    return out->role == role ? 0 : -1;
}

static int staff_write_locked(const Staff *st) {
    if (st->staff_id <= 0 || st->staff_id > MAX_STAFF) return -1;
    off_t offset = staff_offset(st->role, st->staff_id);
    if (offset < 0) return -1;
    ssize_t n = pwrite(g_staff_fd, st, sizeof(Staff), offset);
    if (n != (ssize_t)sizeof(Staff)) return -1;
    return fdatasync(g_staff_fd) == 0 ? 0 : -1;
}

int storage_add_staff(Staff *st) {
    if (!st || (st->role != STAFF_EMPLOYEE && st->role != STAFF_MANAGER)) return -1;
    pthread_mutex_lock(&g_staff_creation_mutex);
    int id = -1;
    for (int i = 1; i <= MAX_STAFF; i++) {
        Staff tmp;
        if (staff_read_locked(st->role, i, &tmp) != 0) { pthread_mutex_unlock(&g_staff_creation_mutex); return -1; }
        if (tmp.status == STATUS_EMPTY || tmp.status == STATUS_DELETED) { id = i; break; }
    }
    if (id < 0) { pthread_mutex_unlock(&g_staff_creation_mutex); return -1; }
    st->staff_id = id;
    RecordLockHandle staff_lock;
    if (lock_table_acquire(LOCK_RECORD_STAFF, storage_staff_lock_id(st->role, id), &staff_lock) != 0) {
        pthread_mutex_unlock(&g_staff_creation_mutex);
        return -1;
    }
    rwlock_acquire_write(staff_lock.lock);
    int rc = staff_write_locked(st);
    rwlock_release_write(staff_lock.lock);
    lock_table_release(&staff_lock);
    pthread_mutex_unlock(&g_staff_creation_mutex);
    return rc;
}

int storage_read_staff(StaffRole role, int staff_id, Staff *out) {
    return staff_read_locked(role, staff_id, out);
}

int storage_update_staff(StaffRole role, const Staff *st) {
    if (!st || st->role != role) return -1;
    return staff_write_locked(st);
}

int storage_change_staff_role(StaffRole from_role, int staff_id, StaffRole to_role) {
    if ((from_role != STAFF_EMPLOYEE && from_role != STAFF_MANAGER) ||
        (to_role != STAFF_EMPLOYEE && to_role != STAFF_MANAGER) ||
        from_role == to_role || staff_id <= 0 || staff_id > MAX_STAFF) return -1;

    pthread_mutex_lock(&g_staff_creation_mutex);
    Staff source;
    if (staff_read_locked(from_role, staff_id, &source) != 0 ||
        source.status == STATUS_EMPTY || source.status == STATUS_DELETED) {
        pthread_mutex_unlock(&g_staff_creation_mutex);
        return -1;
    }

    int target_id = -1;
    for (int i = 1; i <= MAX_STAFF; i++) {
        Staff target;
        if (staff_read_locked(to_role, i, &target) != 0) {
            pthread_mutex_unlock(&g_staff_creation_mutex);
            return -1;
        }
        if (target.status == STATUS_EMPTY || target.status == STATUS_DELETED) {
            target_id = i;
            break;
        }
    }
    if (target_id < 0) {
        pthread_mutex_unlock(&g_staff_creation_mutex);
        return -1;
    }

    int source_lock_id = storage_staff_lock_id(from_role, staff_id);
    int target_lock_id = storage_staff_lock_id(to_role, target_id);
    int first_id = source_lock_id < target_lock_id ? source_lock_id : target_lock_id;
    int second_id = source_lock_id < target_lock_id ? target_lock_id : source_lock_id;
    RecordLockHandle first_lock, second_lock;
    if (lock_table_acquire(LOCK_RECORD_STAFF, first_id, &first_lock) != 0) {
        pthread_mutex_unlock(&g_staff_creation_mutex);
        return -1;
    }
    if (lock_table_acquire(LOCK_RECORD_STAFF, second_id, &second_lock) != 0) {
        lock_table_release(&first_lock);
        pthread_mutex_unlock(&g_staff_creation_mutex);
        return -1;
    }
    rwlock_acquire_write(first_lock.lock);
    rwlock_acquire_write(second_lock.lock);

    Staff moved;
    Staff destination;
    if (staff_read_locked(from_role, staff_id, &moved) != 0 ||
        moved.status == STATUS_EMPTY || moved.status == STATUS_DELETED ||
        staff_read_locked(to_role, target_id, &destination) != 0 ||
        (destination.status != STATUS_EMPTY && destination.status != STATUS_DELETED)) {
        rwlock_release_write(second_lock.lock);
        rwlock_release_write(first_lock.lock);
        lock_table_release(&second_lock);
        lock_table_release(&first_lock);
        pthread_mutex_unlock(&g_staff_creation_mutex);
        return -1;
    }
    Staff original = moved;
    moved.role = to_role;
    moved.staff_id = target_id;
    int rc = staff_write_locked(&moved);
    if (rc == 0) {
        original.status = STATUS_DELETED;
        rc = staff_write_locked(&original);
        if (rc != 0) {
            moved.status = STATUS_DELETED;
            (void)staff_write_locked(&moved);
        }
    }

    rwlock_release_write(second_lock.lock);
    rwlock_release_write(first_lock.lock);
    lock_table_release(&second_lock);
    lock_table_release(&first_lock);
    pthread_mutex_unlock(&g_staff_creation_mutex);
    return rc;
}

static int migrate_legacy_staff_records(void) {
    for (int old_id = 1; old_id <= MAX_STAFF; old_id++) {
        Staff legacy;
        off_t old_offset = (off_t)old_id * (off_t)sizeof(Staff);
        ssize_t n = pread(g_staff_fd, &legacy, sizeof(legacy), old_offset);
        if (n == 0) continue;
        if (n != (ssize_t)sizeof(legacy)) return -1;
        if (legacy.role != STAFF_MANAGER || legacy.status == STATUS_EMPTY) continue;

        int new_id = old_id == 2 && strcmp(legacy.name, "manager") == 0 ? 1 : old_id;
        Staff existing;
        if (staff_read_locked(STAFF_MANAGER, new_id, &existing) != 0) return -1;
        if (existing.status == STATUS_EMPTY) {
            legacy.staff_id = new_id;
            if (staff_write_locked(&legacy) != 0) return -1;
            Staff empty;
            memset(&empty, 0, sizeof(empty));
            empty.staff_id = old_id;
            if (pwrite(g_staff_fd, &empty, sizeof(empty), old_offset) != (ssize_t)sizeof(empty) ||
                fdatasync(g_staff_fd) != 0) return -1;
        }
    }
    return 0;
}

/* ---------- loans ---------- */

static int loan_read_locked(int loan_id, Loan *out) {
    if (loan_id <= 0 || loan_id > MAX_LOANS) return -1;
    off_t offset = (off_t)loan_id * (off_t)sizeof(Loan);
    ssize_t n = pread(g_loans_fd, out, sizeof(Loan), offset);
    if (n == 0) {
        memset(out, 0, sizeof(Loan));
        out->loan_id = loan_id;
        out->status = LOAN_EMPTY;
        return 0;
    }
    return (n == (ssize_t)sizeof(Loan)) ? 0 : -1;
}

static int loan_write_locked(const Loan *ln) {
    if (ln->loan_id <= 0 || ln->loan_id > MAX_LOANS) return -1;
    off_t offset = (off_t)ln->loan_id * (off_t)sizeof(Loan);
    ssize_t n = pwrite(g_loans_fd, ln, sizeof(Loan), offset);
    if (n != (ssize_t)sizeof(Loan)) return -1;
    return fdatasync(g_loans_fd) == 0 ? 0 : -1;
}

int storage_add_loan(Loan *ln) {
    pthread_mutex_lock(&g_loan_creation_mutex);
    int id = -1;
    for (int i = 1; i <= MAX_LOANS; i++) {
        Loan tmp;
        if (loan_read_locked(i, &tmp) != 0) { pthread_mutex_unlock(&g_loan_creation_mutex); return -1; }
        if (tmp.status == LOAN_EMPTY) { id = i; break; }
    }
    if (id < 0) { pthread_mutex_unlock(&g_loan_creation_mutex); return -1; }
    ln->loan_id = id;
    RecordLockHandle loan_lock;
    if (lock_table_acquire(LOCK_RECORD_LOAN, id, &loan_lock) != 0) {
        pthread_mutex_unlock(&g_loan_creation_mutex);
        return -1;
    }
    rwlock_acquire_write(loan_lock.lock);
    int rc = loan_write_locked(ln);
    rwlock_release_write(loan_lock.lock);
    lock_table_release(&loan_lock);
    pthread_mutex_unlock(&g_loan_creation_mutex);
    return rc;
}

int storage_read_loan(int loan_id, Loan *out) {
    return loan_read_locked(loan_id, out);
}

int storage_update_loan(const Loan *ln) {
    return loan_write_locked(ln);
}

int storage_for_each_loan(int (*visit)(const Loan *ln, void *ctx), void *ctx) {
    for (int i = 1; i <= MAX_LOANS; i++) {
        RecordLockHandle loan_lock;
        if (lock_table_acquire(LOCK_RECORD_LOAN, i, &loan_lock) != 0) continue;
        rwlock_acquire_read(loan_lock.lock);
        Loan ln;
        int rc = loan_read_locked(i, &ln);
        int keep_going = rc == 0 && ln.status != LOAN_EMPTY ? visit(&ln, ctx) : 1;
        rwlock_release_read(loan_lock.lock);
        lock_table_release(&loan_lock);
        if (!keep_going) break;
    }
    return 0;
}

/* ---------- feedback ---------- */

static int feedback_read_locked(int feedback_id, Feedback *out) {
    if (feedback_id <= 0 || feedback_id > MAX_FEEDBACK) return -1;
    off_t offset = (off_t)feedback_id * (off_t)sizeof(Feedback);
    ssize_t n = pread(g_feedback_fd, out, sizeof(Feedback), offset);
    if (n == 0) {
        memset(out, 0, sizeof(Feedback));
        out->feedback_id = feedback_id;
        out->status = FEEDBACK_EMPTY;
        return 0;
    }
    return (n == (ssize_t)sizeof(Feedback)) ? 0 : -1;
}

static int feedback_write_locked(const Feedback *fb) {
    if (fb->feedback_id <= 0 || fb->feedback_id > MAX_FEEDBACK) return -1;
    off_t offset = (off_t)fb->feedback_id * (off_t)sizeof(Feedback);
    ssize_t n = pwrite(g_feedback_fd, fb, sizeof(Feedback), offset);
    if (n != (ssize_t)sizeof(Feedback)) return -1;
    return fdatasync(g_feedback_fd) == 0 ? 0 : -1;
}

int storage_add_feedback(Feedback *fb) {
    pthread_mutex_lock(&g_feedback_mutex);
    int id = -1;
    for (int i = 1; i <= MAX_FEEDBACK; i++) {
        Feedback tmp;
        if (feedback_read_locked(i, &tmp) != 0) { pthread_mutex_unlock(&g_feedback_mutex); return -1; }
        if (tmp.status == FEEDBACK_EMPTY) { id = i; break; }
    }
    if (id < 0) { pthread_mutex_unlock(&g_feedback_mutex); return -1; }
    fb->feedback_id = id;
    int rc = feedback_write_locked(fb);
    pthread_mutex_unlock(&g_feedback_mutex);
    return rc;
}

int storage_read_feedback(int feedback_id, Feedback *out) {
    pthread_mutex_lock(&g_feedback_mutex);
    int rc = feedback_read_locked(feedback_id, out);
    pthread_mutex_unlock(&g_feedback_mutex);
    return rc;
}

int storage_update_feedback(const Feedback *fb) {
    pthread_mutex_lock(&g_feedback_mutex);
    int rc = feedback_write_locked(fb);
    pthread_mutex_unlock(&g_feedback_mutex);
    return rc;
}

int storage_for_each_feedback(int (*visit)(const Feedback *fb, void *ctx), void *ctx) {
    pthread_mutex_lock(&g_feedback_mutex);
    for (int i = 1; i <= MAX_FEEDBACK; i++) {
        Feedback fb;
        if (feedback_read_locked(i, &fb) != 0) continue;
        if (fb.status == FEEDBACK_EMPTY) continue;
        if (!visit(&fb, ctx)) break;
    }
    pthread_mutex_unlock(&g_feedback_mutex);
    return 0;
}
