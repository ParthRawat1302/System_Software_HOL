# Banking Management System

A banking system built Customer/Employee/Manager/Administrator roles, loan
processing, fund transfers with ACID properties, semaphores. Everything
either brief asks for is implemented in a **standalone C program**
(`engine/`) — no database library, no crypto library, no premade locking
API. A Python gateway and a React frontend sit on top purely for a
modern web UI; they contain no banking logic of their own.

```
React (browser) --REST/JSON--> Python gateway --custom socket protocol--> C engine
                                (FastAPI)                                  (raw sockets, pthreads,
                                                                             semaphores,
                                                                             WAL recovery)
```

`engine/bankclient` is a second, independent client that speaks the same
protocol from the terminal — the engine is fully usable with zero
Python/React involved, which is the point.

## Roles

Four roles, matching the CS-513 brief's structure. Every capability
listed is implemented and covered by an automated test
(`tests/test_engine.py`); ✅ marks each.

**Customer** (`USER` login, the existing normal/joint account holder)
- ✅ Login (one session per user — see [below](#one-session-per-user))
- ✅ View Account Balance · ✅ Deposit · ✅ Withdraw
- ✅ Transfer Funds (atomic, ACID — see [ACID funds transfer](#acid-funds-transfer))
- ✅ Apply for a Loan · ✅ Change Password · ✅ Add Feedback
- ✅ View Transaction History (with per-owner attribution on joint accounts) · ✅ Logout · ✅ Exit

**Bank Employee** (`EMPLOYEE` login)
- ✅ Login (one session per user) · ✅ Add New Customer · ✅ Modify/Close Customer Accounts
- ✅ Process Loan Applications assigned to them · ✅ Approve/Reject Loans
- ✅ View Assigned Loan Applications · ✅ View Customer Transactions (passbook)
- ✅ Change Password · ✅ Logout · ✅ Exit

**Manager** (`MANAGER` login)
- ✅ Login (one session per user) · ✅ Review unassigned loan requests · ✅ Activate/Deactivate Customer Accounts
- ✅ Assign Loan Application Processes to Employees · ✅ Review Customer Feedback
- ✅ Change Password · ✅ Logout · ✅ Exit

**Administrator** (`ADMIN` login, superset access)
- ✅ Login (one session per user) · ✅ Add New Bank Employee (or Manager)
- ✅ Modify Customer/Employee Details · ✅ Manage User Roles (promote/demote Employee ↔ Manager, activate/deactivate staff)
- ✅ Change Password · ✅ Logout · ✅ Exit

The consolidated <a href="https://htmlpreview.github.io/?https://github.com/ParthRawat1302/System_Software_HOL/blob/main/Banking_System/docs/report.html" target="_blank" rel="noopener noreferrer">project report</a> includes the full
permission matrix, wire-protocol reference, and diagrams of the
`Account`/`Staff`/`Admin`/`Loan`/`Feedback` data model.

## Socket programming & system calls

Both briefs require this, worded almost identically — quoting CS-513's
version since it's the more recent/specific one:

> **Socket Programming:** The server maintains the database and serves
> multiple clients concurrently. Clients can connect to the server and
> access their specific account details.
>
> **System Calls:** Use system calls instead of library functions
> wherever possible, including process management, file management,
> file locking, semaphores, multithreading, and inter-process
> communication mechanisms.

- `socket()`, `bind()`, `listen()`, `accept()` — `engine/src/server.c`
  (`server_run()`). One `accept()` loop, one worker thread per
  connection, gated by a counting semaphore (see below).
- The "database" is `engine/data/*.dat`: fixed-size binary records read
  and written directly with syscalls, no database engine or library,
  each addressed directly by id (`offset = id * sizeof(record)`).
- The client program is `engine/bankclient`
  (`engine/src/roles/customer/client.c`): it
  `connect()`s over a real TCP socket, logs in, and every request after
  that is scoped to that one session's identity (`ClientSession`,
  `engine/include/session.h`) — a logged-in client can only ever read or
  modify *their own* account (or, for staff, whatever their role
  authorizes), enforced server-side on every opcode, never trusted from
  the client.

| Category | System calls | Where | Library function avoided |
|---|---|---|---|
| **File management** | `open()`, `pread()`, `pwrite()`, `fdatasync()` | `engine/src/storage.c` | `fopen()`/`fread()`/`fwrite()` (stdio buffered I/O) |
| **File locking** | *(deliberately not a syscall — see below)* | `engine/src/rwlock.c` | `fcntl()`/`flock()` **and** `pthread_rwlock_t` |
| **Semaphores** | `sem_init()`, `sem_wait()`, `sem_post()` | `engine/src/server.c` | — (POSIX semaphores are the mechanism itself) |
| **Multithreading** | `pthread_create()`, `pthread_detach()` | `engine/src/server.c` | — (POSIX threads are the mechanism itself) |

Four of these deserve a sentence each:

- **File management** uses `pread()`/`pwrite()` specifically, not
  `lseek()` + `read()`/`write()` (a shared file descriptor across every
  client thread means a plain `lseek()` could race against another
  thread's), and every write is followed by `fdatasync()` for durability
  — see [ACID funds transfer](#acid-funds-transfer).
- **File locking** is the one category where using the "obvious" system
  call (`fcntl()`/`flock()`) would have been *wrong* for this
  assignment: that's the OS's own pre-built locking service, and using
  it would mean the joint-account locking requirement was implemented by
  the kernel, not by us. So instead `engine/src/rwlock.c` hand-rolls the
  actual readers–writers algorithm from lower-level primitives
  (`pthread_mutex_t` + `pthread_cond_t`) — see [Locking](#locking) below.
- **Semaphores** bound how many clients the server serves at once
  (`MAX_CONNECTIONS`, 50): `sem_wait()` gates every `accept()`, `sem_post()`
  runs when a client thread exits, so a connection flood queues in the
  kernel's backlog instead of spawning unbounded threads. Accepted engine
  sockets also have a 10-second idle read timeout by default; set
  `ENGINE_IDLE_TIMEOUT_SECONDS` to a positive number of seconds to override it.
- **Recovery** uses a write-ahead log (WAL): each financial operation
  writes its before- and after-images durably before account and ledger
  files change. Startup recovery redoes committed operations and undoes
  interrupted ones before the server accepts clients.

## Quick start

Three separate runtimes. The engine needs POSIX threads, semaphores, and
raw sockets, so it must run on Linux/WSL even if you develop on
Windows; the gateway and frontend run fine natively on Windows.

**1. C engine** (inside WSL/Linux):
```bash
cd engine or
cd "/mnt/c/Users/Parth/Desktop/System Design/Banking_System/engine"
make clean && make bankserver
./bankserver
```
Transaction timestamps use `Asia/Kolkata` by default, including under WSL.
Set `TZ` before starting the server to use another timezone, for example
`TZ=UTC ./bankserver`. Existing ledger entries keep the timestamps recorded
when those transactions were made.
On startup, empty demo slots are seeded for customer `user` (ID 1), Employee
`staff` (ID 1), Manager `manager` (ID 1), and admin `admin` (ID 1), all with
password `pass123`. Employee and Manager IDs use separate namespaces. The
customer starts with a zero balance. Existing records are not overwritten.
These simple credentials are for local testing only. Data files land in
`engine/data/`.

Test the engine directly, with zero Python/React involved:
```bash
./bankclient
```
The terminal client prompts for login first and then displays only the
options available to the authenticated Customer, Employee, Manager, or
Administrator role.

Run the automated suite (builds, starts, runs 66 checks — including a
500-op concurrency race on the hand-rolled lock, a 300-transfer
bidirectional ACID/deadlock stress test, one-session-per-user
enforcement, and 8 security regression tests — then shuts down cleanly):
```bash
make test
```

**2. Python gateway** (Windows or WSL):
```bash
cd gateway
python -m venv venv
./venv/Scripts/pip install -r requirements.txt   # ./venv/bin/pip on WSL/Linux
./venv/Scripts/python -m uvicorn app.main:app --port 8077
```
Port 8077 is arbitrary — pick any free one and update the frontend's
`VITE_API_URL`/`BASE_URL` to match if you change it. Ports 8000/8001 are
common defaults and may already be in use by something else on your
machine.

> **If this venv was ever created from inside WSL against this same
> project folder** (e.g. `python3 -m venv venv` run from a Linux shell),
> its `pyvenv.cfg` will point at a Linux interpreter path and every
> Windows invocation will fail with something like `did not find
> executable at '/usr/bin\python.exe'`. Fix: delete `gateway/venv` and
> recreate it with the Windows `python` you intend to run it with. (This
> happens more than once if the project folder is opened from WSL in
> between — worth checking first if the gateway suddenly won't start.)

**3. React frontend:**
```bash
cd frontend
npm install
npm run dev
```
Open the printed `localhost` URL — note it can land on `5173`, `5174`,
etc. depending on what else is running. The frontend calls the gateway
at `http://localhost:8077` specifically, **not** `127.0.0.1` — browsers
treat those as different sites, so a `127.0.0.1` gateway URL would make
the session cookie silently fail to round-trip when the page itself is
served from `localhost`.

## Project structure

```
engine/       C banking engine (the graded part) -- see docs/report.html
  src/, include/    server, storage, hand-rolled rwlock, auth_guard,
                     session_guard, WAL recovery, protocol
  Makefile          `make`, `make test`
  bankserver        socket server (built)
  bankclient        standalone CLI client (built)
  data/             accounts.dat, admins.dat, staff.dat, loans.dat,
                     feedback.dat, wal.dat, loan_wal.dat, ledger.dat (created at runtime)
gateway/      FastAPI translator, no banking logic -- see gateway/app/main.py
frontend/     React (Vite) UI -- one dashboard per role
tests/        tests/test_engine.py -- protocol-level automated suite
docs/
  report.html   consolidated EG 301 report, architecture/design diagrams,
                 ACID and WAL details, security review, full wire protocol,
                 authorization matrix, and protocol invariants
  VIVA_GUIDE.md oral-defense notes and detailed request-lifecycle walkthrough
```

## Locking

Joint-account read/write locking is deliberately **not** implemented
with `fcntl()`/`flock()` or `pthread_rwlock_t` — those are the OS's/
libc's own pre-built locking service, which would defeat the point of an
OS project about implementing the mechanism yourself. Instead
`engine/src/rwlock.c` hand-rolls the classic writer-priority
readers–writers algorithm from `pthread_mutex_t` + `pthread_cond_t`. The
lock table creates a per-record lock on demand and removes it when idle.
This was load-tested directly — 500 concurrent
`+1` deposits fired at the same joint account from both holders
simultaneously landed on exactly the expected balance, zero lost
updates. Full design and lock details are in
[`docs/report.html`](docs/report.html#design-reference).

## ACID funds transfer

Transferring money between two accounts is the one operation that
touches two records at once, which is exactly the case a single-account
lock doesn't cover — and exactly what the CS-513 brief's evaluation
criteria name explicitly ("Concurrency, synchronization, and ACID
properties"). `engine/src/server.c`'s `handle_transfer()`:

- **Atomicity** — both accounts' locks are held for the whole
  validate-then-write section; any failed check aborts before either
  balance is written.
- **Isolation** — no other operation on either account can interleave
  while a transfer is in flight, since both write locks are held for its
  entire duration.
- **Consistency** — the debit and credit are always the same amount, so
  the combined balance of both accounts is invariant.
- **Durability** — every write is followed by `fdatasync()`, so a
  committed leg survives a crash immediately after.
- **Deadlock-free** — both locks are always acquired in ascending
  account-id order, regardless of transfer direction, so two transfers
  running in opposite directions at once can never form a circular wait.
  Proven directly: `tests/test_engine.py` fires 300 transfers in both
  directions between the same two accounts concurrently and asserts
  neither thread hangs and the combined balance is exactly conserved.

Full transfer, WAL, and recovery discussion is in
[`docs/report.html`](docs/report.html#design-reference).

## One session per user

Every role in the brief lists "Login (one session per user)".
`engine/src/common/session_guard.c` enforces it: a second concurrent login for
the same identity is rejected. On a joint account the two owners are
different identities (keyed by account id *and* which owner's password
matched), so they can still both be logged in at once — only the same
owner logging in twice concurrently is blocked. Logging out releases the
identity *before* the response is sent, so a client is guaranteed it can
log back in immediately, with no race window. The gateway does not log out
an existing engine session to make a duplicate login succeed. The React UI
restores its role after a page refresh in the same tab; closing the browser
without logging out leaves the engine session reserved until the C engine's
10-second idle socket timeout closes the connection.

## Transaction history

Every deposit/withdraw/transfer is recorded in the durable shared
`engine/data/ledger.dat` file
with a timestamp and *who* made it — on a joint account, that's the
actual holder's name (e.g. `alice` vs `bob`), not just
"owner1"/"owner2". A logged-in customer sees their own account's last 20
transactions (CLI option 7, React's "Transaction History", or `GET
/api/history`); an Employee or Administrator can pull any account's
history by id ("passbook", CLI option 13, React's "Customer Passbook",
or `GET /api/admin/accounts/{id}/history`). Transfer legs carry a
`counterparty` field naming the other account. See the
[`TXN_HISTORY` protocol reference](docs/report.html#protocol-reference)
for the wire format and WAL recovery guarantee.

## Security

The [consolidated report](docs/report.html#security) and its
[protocol invariants](docs/report.html#protocol-reference) cover this in detail. Summary of what was found
and fixed during development (each with a regression test in
`tests/test_engine.py`):

- **NaN/Infinity balance injection** — `atof("nan")` bypassed the
  `amount <= 0` guard (NaN comparisons are always false in C) and
  corrupted a balance to NaN, permanently, on disk. Fixed with a strict
  `strtod`-based parser that rejects non-finite results.
- **Wire-protocol delimiter injection** — a value containing the
  protocol's own `|` field separator silently shifted every later field
  with no error. Fixed by checking the exact `|` count before parsing,
  plus an outright ban on `|`/`;` in stored values.
- **No brute-force protection** — unlimited password guessing. Fixed
  with a 5-attempts/30-second lockout that skips the password check
  entirely while locked. (Note: lockout is keyed by identity, so on a
  joint account, one holder's failed attempts lock out the other too —
  intentional, not a bug.)
- **Unnecessary network exposure** — the engine listened on `0.0.0.0`;
  now `127.0.0.1` only.
- **Abandoned engine connections** — the C engine closes client sockets
  after 10 seconds idle by default. The gateway keeps only an in-memory
  token-to-connection mapping; it clears entries on logout, replacement
  login, or shutdown, not on an independent idle timer. If a request later
  discovers that the engine expired its socket, the gateway removes the
  stale mapping and the frontend returns to the login screen.
- **Unbounded connection count** — an accept loop with no limit could be
  flooded into spawning unbounded threads. Closed by the connection-
  limiting semaphore described above.

Known, deliberate trade-offs (documented, not fixed, given this is
coursework rather than a production system): no transport encryption
(plaintext protocol, loopback-only by design) and an unsalted hand-rolled
FNV-1a password hash (chosen specifically to avoid an external
crypto library rather than for real security).

## Testing

```bash
cd engine && make test
```
66 protocol-level checks against a real running instance of the engine:
login/deposit/withdraw/password/admin CRUD, the joint-account
concurrency race, 8 security regressions, transaction-history/owner-
attribution checks, Employee/Manager/Admin role boundaries, one-session-
per-user enforcement, and the 300-transfer bidirectional ACID/deadlock
stress test. No mocking — it builds the real binary, starts it as a
subprocess, and talks to it over the actual socket protocol.

The frontend has also been driven end-to-end in a real (headless)
browser during development — login → dashboard → each menu action →
logout, for all four roles, including the full Customer→Manager→Employee
loan-assignment handoff and a fund transfer between two real accounts.

## Demo accounts

Startup seeds these local test logins when their ID-1 records are empty:

| Role | ID | Name | Password |
|---|---:|---|---|
| Customer | 1 | `user` | `pass123` |
| Employee | 1 | `staff` | `pass123` |
| Manager | 2 | `manager` | `pass123` |
| Admin | 1 | `admin` | `pass123` |

The login uses role, ID, and password; the name is a display name. These
weak demo credentials are for local testing only. Create additional
accounts/staff via the relevant dashboard, `bankclient`, or directly:

```bash
curl -X POST http://localhost:8077/api/login \
  -H "Content-Type: application/json" \
  -d '{"kind":"ADMIN","id":1,"password":"pass123"}' -c cookies.txt

curl -X POST http://localhost:8077/api/staff \
  -H "Content-Type: application/json" -b cookies.txt \
  -d '{"role":"EMPLOYEE","name":"bob","password":"emp123"}'

curl -X POST http://localhost:8077/api/admin/accounts \
  -H "Content-Type: application/json" -b cookies.txt \
  -d '{"role":"JOINT","owner1_name":"amit","owner1_password":"amit123","owner2_name":"neha","owner2_password":"neha123","initial_balance":10000}'
```

## Documentation

- **[`docs/report.html`](docs/report.html) — consolidated project report.**
  Architecture and data-model diagrams, detailed design, ACID transfers,
  WAL recovery, concurrency, code-based bottleneck analysis and practical
  future work, security findings, all protocol opcodes, authorization,
  and protocol invariants.
