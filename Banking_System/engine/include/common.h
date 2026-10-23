#ifndef COMMON_H
#define COMMON_H

#define NAME_LEN 64
#define TEXT_LEN 256
#define MAX_ACCOUNTS 1000
#define MAX_ADMINS 50
#define MAX_STAFF 200
#define MAX_LOANS 1000
#define MAX_FEEDBACK 1000
#define MAX_PAYLOAD 4096
#define LOG_MSG_LEN 256
#define SERVER_PORT 9090

/* Counting semaphore limit on simultaneously-served client connections
 * (see server_run() in server.c) -- bounds how many client threads can
 * exist at once, so a flood of connections can't exhaust memory/threads. */
#define MAX_CONNECTIONS 50

#define DATA_DIR "data"
#define ACCOUNTS_FILE  DATA_DIR "/accounts.dat"
#define ADMINS_FILE    DATA_DIR "/admins.dat"     /* Administrator credential store */
#define STAFF_FILE     DATA_DIR "/staff.dat"       /* Employee & Manager credential store */
#define LOANS_FILE     DATA_DIR "/loans.dat"
#define FEEDBACK_FILE  DATA_DIR "/feedback.dat"
#define WAL_FILE       DATA_DIR "/wal.dat"
#define LOAN_WAL_FILE  DATA_DIR "/loan_wal.dat"
#define LEDGER_FILE    DATA_DIR "/ledger.dat"

typedef enum { ROLE_EMPTY = 0, ROLE_NORMAL = 1, ROLE_JOINT = 2 } AccountRole;

/* STATUS_INACTIVE is a Manager-controlled suspension (reversible), distinct
 * from STATUS_DELETED (an Administrator/Employee action, permanent for the
 * purposes of this project). Login rejects anything that isn't ACTIVE, so
 * INACTIVE behaves like DELETED for login purposes but can be reactivated. */
typedef enum { STATUS_EMPTY = 0, STATUS_ACTIVE = 1, STATUS_DELETED = 2, STATUS_INACTIVE = 3 } AccountStatus;

/* Fixed-size record -> direct lseek(id * sizeof(Account)) addressing, no index/library DB. */
typedef struct {
    int account_id;
    AccountRole role;
    AccountStatus status;
    char owner1_name[NAME_LEN];
    unsigned long owner1_pw_hash;
    char owner2_name[NAME_LEN];   /* only meaningful when role == ROLE_JOINT */
    unsigned long owner2_pw_hash;
    double balance;
} Account;

typedef struct {
    int admin_id;
    AccountStatus status;
    char name[NAME_LEN];
    unsigned long pw_hash;
} Admin;

typedef enum { STAFF_EMPTY = 0, STAFF_EMPLOYEE = 1, STAFF_MANAGER = 2 } StaffRole;

typedef struct {
    int staff_id;
    StaffRole role;
    AccountStatus status;   /* ACTIVE / DELETED (reuses the same enum) */
    char name[NAME_LEN];
    unsigned long pw_hash;
} Staff;

typedef enum { LOAN_EMPTY = 0, LOAN_PENDING = 1, LOAN_APPROVED = 2, LOAN_REJECTED = 3 } LoanStatus;

typedef struct {
    int loan_id;
    int account_id;             /* applicant */
    double amount;
    char purpose[TEXT_LEN];
    LoanStatus status;
    int assigned_employee_id;   /* 0 = unassigned, set by a Manager */
    int decided_by;             /* staff_id who approved/rejected, 0 if still pending */
} Loan;

typedef enum { FEEDBACK_EMPTY = 0, FEEDBACK_ACTIVE = 1 } FeedbackStatus;

typedef struct {
    int feedback_id;
    int account_id;
    FeedbackStatus status;
    int reviewed;
    char text[TEXT_LEN];
} Feedback;

/* An append-only, fixed-size passbook record.  It is written as part of
 * the same WAL transaction as the account balance it describes. */
typedef struct {
    unsigned long long transaction_id;
    char timestamp[32];
    int account_id;
    char owner[NAME_LEN];
    char operation[20];
    double amount;
    double balance_after;
    int counterparty_account_id; /* 0 means no counterparty */
} LedgerEntry;

#endif
