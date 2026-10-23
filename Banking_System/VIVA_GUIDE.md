# Viva Guide — Architecture, Trade-offs and Failure Analysis

This is the "defend the project" document. It covers what every part does,
**why it was built this way and not some other way**, exactly where the
system breaks, why it breaks there, and what the fix would be.

Everything labelled **MEASURED** was actually executed against the running
system — commands and results are reproducible. Everything labelled
**ANALYSED** is reasoned from the code but not demonstrated; those are
flagged honestly rather than dressed up as proven.

**Current state:** 68/68 automated tests pass (`cd engine && make test`).
Three defects were found by deliberately attacking the system while
writing this document; all three are now fixed and each has a regression
test. They are written up in full in §7 — knowing how you found and fixed
a bug is usually worth more marks than claiming you never had one.

---

## 1. The 60-second answer

> "It's a three-tier system. The bottom tier is a C engine that owns all
> the data and all the concurrency — raw TCP sockets, one POSIX thread per
> client, a hand-written readers–writer lock per account, a forked logger
> process fed by a System V message queue, and a counting semaphore that
> caps concurrent connections. On top there's a Python FastAPI gateway
> that is deliberately dumb: it translates JSON to the engine's wire
> protocol and back, and holds zero banking logic. On top of that a React
> UI with one dashboard per role. The engine is fully usable on its own —
> `bankclient` is a terminal client that speaks the same protocol — so the
> web tiers can be removed without losing any graded functionality."

Ports: engine `127.0.0.1:9090` (loopback only), gateway `:8077`, UI `:5173/5174`.

---

## 2. Request lifecycle — trace one deposit end to end

A viva favourite: *"walk me through what happens when a customer deposits ₹100."*

1. **Browser** — `UserDashboard.jsx` calls `api.deposit(100)` →
   `fetch(POST /api/deposit, {credentials:"include"})`, so the session
   cookie rides along.
2. **Gateway** (`gateway/app/main.py`) — `require_session()` reads the
   `session_token` cookie, looks up the live `EngineConnection` in
   `session_store` (this also refreshes its idle timer). No banking logic
   runs here.
3. **Gateway → engine** — `EngineConnection.request()` takes that
   connection's own mutex (one in-flight request per session), frames the
   message as `[4-byte big-endian length][opcode][payload]` and `sendall`s
   it over the already-open TCP socket.
4. **Engine** — that socket already has a dedicated worker thread parked
   in `recv_frame()`. `read_full()` loops until the whole header, opcode
   and payload have arrived (TCP gives a byte stream, not messages — the
   length prefix is what re-creates message boundaries).
5. **Dispatch** — `client_thread()`'s switch routes opcode 2 to
   `handle_deposit()`.
6. **Authorisation** — `sess.role != SESSION_USER` → reject. The role came
   from the *server-side* session struct, never from the client.
7. **Validation** — `parse_amount()` (strict `strtod`) rejects `nan`,
   `inf`, garbage and trailing junk. `atof()` would not.
8. **Critical section** — `lock_table_get(account_id)` →
   `rwlock_acquire_write()` → `pread()` the 168-byte record at
   `id × 168` → check `STATUS_ACTIVE` → `balance += amount` → `pwrite()` →
   `fdatasync()` → `rwlock_release_write()`. Read-modify-write happens
   entirely inside the lock, which is what makes lost updates impossible.
9. **Audit** — build a `TXN|timestamp|account|owner|DEPOSIT|amount|balance|-`
   line; `msgsnd()` it to the System V queue. The **forked logger process**
   is blocked in `msgrcv()`, wakes, and appends it to `txn_log.dat` with a
   single `write()`.
10. **Reply** — `send_frame()` writes `OK|<new balance>` back.
11. **Back up the stack** — gateway parses `OK|…` → `{"ok":true,"balance":…}` →
    React re-renders.

Name-drop for marks: *length-prefixed framing, server-side authorisation,
read-modify-write under an exclusive lock, `fdatasync` for durability,
IPC hand-off so the transaction isn't slowed by disk logging.*

---

## 3. Component map

### Engine (`engine/`) — everything graded lives here

| File | Responsibility |
|---|---|
| `main.c` | Startup order: storage → locks → guards → IPC queue → `fork()` logger → install signal handlers → `server_run()`. Also `signal(SIGPIPE, SIG_IGN)` (see §7 F1). |
| `server.c` | The `accept()` loop, the connection semaphore, one thread per client, every opcode handler, all authorisation checks. The biggest file — it *is* the business logic. |
| `storage.c` | All file I/O. `pread`/`pwrite` on fixed-size records + `fdatasync`. Owns the per-file mutexes for staff/loans/feedback. |
| `rwlock.c` | The hand-written writer-priority readers–writer lock (mutex + 2 condition variables). |
| `lock_table.c` | One `RWLock` per account slot, plus the account-creation mutex. |
| `auth_guard.c` | Brute-force lockout: 5 failures → 30s refusal without checking the password. |
| `session_guard.c` | "One session per user" presence table. |
| `journal.c` | Durable WAL, committed ledger writes, and startup redo/undo recovery. |
| `protocol.c` | Framing: `send_frame`/`recv_frame` with partial-I/O loops. |
| `util.c` | FNV-1a hash, field splitting, `parse_amount`, reserved-character checks. |
| `client.c` | `bankclient` — standalone terminal client, proves the engine needs nothing above it. |

### Gateway (`gateway/app/`)
`main.py` (routes, one per opcode), `engine_client.py` (the wire protocol
in Python), `session_store.py` (cookie → live engine socket, plus the
idle reaper).

### Frontend (`frontend/src/`)
`App.jsx` routes by role to `UserDashboard`, `EmployeeDashboard`,
`ManagerDashboard`, `AdminDashboard`; `api.js` is the only file that talks
to the network.

### Data files (`engine/data/`)

| File | Record | Size | Cap |
|---|---|---|---|
| `accounts.dat` | `Account` | 168 B | 1000 |
| `admins.dat` | `Admin` | 80 B | 50 |
| `staff.dat` | `Staff` | 88 B | 200 |
| `loans.dat` | `Loan` | 288 B | 1000 |
| `feedback.dat` | `Feedback` | 272 B | 1000 |
| `txn_log.dat` | append-only text | — | unbounded |

A fully populated `accounts.dat` is 168,000 bytes. Record *n* always lives
at byte offset `n × sizeof(record)` — that is what makes lookup O(1) with
no index.

---

## 4. Design decisions — why this, and why not the alternative

### 4.1 Three tiers, with all logic in C

**Alternatives:** (a) pure C with no web UI; (b) write the whole thing in
Python/Node; (c) put some validation in the gateway for speed.

**Chosen:** C engine owns everything; gateway translates; React renders.

**Why:** the brief grades OS mechanisms — syscalls, locking, IPC,
threads. Those only exist meaningfully in the C tier. Putting *any*
decision in Python would mean a graded requirement was satisfied by a
library instead of by us. Keeping the gateway logic-free also means the
engine is independently demonstrable (`bankclient`), which is exactly the
brief's "client program can connect to the server".

**Cost:** two extra processes, a custom protocol to maintain, and
duplicated data-shape knowledge in three languages.

### 4.2 Fixed-size binary records, not a database or text

**Alternatives:** SQLite; JSON/CSV files; a log-structured store.

**Why fixed-size binary:** the brief says use file management syscalls and
avoid library functions. SQLite would satisfy "storage" while destroying
the point — it would do the locking, the atomicity and the indexing for
us. Fixed-size records give O(1) addressing (`offset = id × sizeof`) with
zero index structure, which keeps `pread`/`pwrite` as the only access
path.

**Cost:** no variable-length fields, hard caps, no schema evolution
(adding a field changes every offset), and the file is not portable
across compilers/architectures because it embeds struct padding.

### 4.3 `pread`/`pwrite`, not `lseek` + `read`/`write`, not `fopen`

`fopen/fread/fwrite` is the C *library* — explicitly discouraged, and it
buffers, which would break durability guarantees.

`lseek()` then `read()` is two syscalls with shared state: the file
offset lives in the **open file description**, which every thread shares.
Thread A could `lseek` to account 5 and thread B `lseek` to account 9
before A's `read` runs — A then reads the wrong record. `pread`/`pwrite`
take the offset as an argument, so each call is atomic and no extra lock
is needed just to protect the cursor.

### 4.4 Hand-rolled readers–writer lock, not `fcntl`/`flock`/`pthread_rwlock_t`

This is the most likely "why did you do it the hard way" question.

**Why not `fcntl()`/`flock()`:** those *are* the operating system's
file-locking service. Using them means the kernel implemented the
locking requirement, not the student. The brief asks for file locking as a
demonstrated mechanism.

**Why not `pthread_rwlock_t`:** same argument one level up — that's libc's
pre-built readers–writer lock.

**What we built:** `rwlock.c` implements the classic writer-priority
readers–writers algorithm from a mutex and two condition variables.
Writer priority means a waiting writer blocks *new* readers, so a stream
of balance enquiries can't starve a pending withdrawal.

**The honest trade-off, and a likely follow-up question:** our lock is a
*process-local* pthread construct. `fcntl()` locks are *kernel* locks and
work across processes. So our design is only correct because exactly one
engine process owns the data files. Run two `bankserver` processes on the
same directory and the locks protect nothing (see §7 F9). That is a real
limitation created by this choice, and the right answer in a viva is to
say so and then explain that a production build would use `fcntl()` record
locks *in addition*, or put the mutexes in shared memory with
`PTHREAD_PROCESS_SHARED`.

### 4.5 Thread-per-client, not process-per-client or an event loop

| Model | Why not |
|---|---|
| `fork()` per client | Each client would get its own address space, so the in-process `RWLock` table would not be shared — locking would silently stop working. Also much heavier. |
| `select`/`poll`/`epoll` event loop | Scales far better (this is how real servers do it), but a single-threaded event loop cannot block in `rwlock_acquire_write()` without stalling every other client, so all the locking would have to be restructured as async state machines. Much more complex, and it would hide rather than showcase the threading requirement. |
| Thread pool + queue | Strictly better than thread-per-client at scale; bounded threads. We get the important half of this (bounded concurrency) from the semaphore instead. |

**Chosen:** one detached thread per connection, because the brief requires
multithreading and because blocking locks compose naturally with blocking
threads. **Cost:** each thread reserves ~8 MB of virtual stack; thousands
of clients would be impossible (see §7 F–scale).

### 4.6 Forked logger + System V message queue, not just writing the log inline

**Alternatives:** write `txn_log.dat` directly from each worker thread; a
dedicated logging *thread*; a pipe; POSIX message queues; shared memory.

**Why a separate process + SysV queue:** the brief lists *process
management* and *IPC* as separate requirements. A logging thread would
satisfy neither (it's just more threading). Writing inline would satisfy
neither and would also put disk latency inside the account lock. Forking
a logger and feeding it over `msgget/msgsnd/msgrcv` demonstrates both
mechanisms genuinely, and decouples transaction latency from log latency.

**Cost:** the queue is a kernel-global object keyed by `ftok()`; it leaks
if the server is `SIGKILL`ed (§7 F15), and `IPC_NOWAIT` means audit
records are dropped rather than blocking a customer's transaction (§7 F6).

### 4.7 Counting semaphore for the connection cap

A semaphore is the correct primitive here because this is a *counting*
resource problem: N interchangeable slots. A mutex would only allow one.
`sem_wait()` before `accept()`, `sem_post()` when a client thread exits.

**Why not just let threads be unbounded?** That is a trivial DoS — an
attacker opens connections until the process runs out of memory or thread
IDs. **Cost:** because we wait *before* `accept()`, excess clients sit
unanswered in the kernel backlog, which turns into its own availability
problem when connections are held idle (§7 F5).

### 4.8 Session uniqueness via a mutex-guarded table, not a semaphore

"One session per user" is a *presence/uniqueness* question (is this exact
identity already in use?), not a counting one, so a boolean table under a
mutex is the honest primitive. Keyed by `(account_id, owner_slot)` for
customers so that **the two owners of a joint account can still be logged
in at the same time** — which matters, because the joint-account
concurrency demo depends on it.

### 4.9 Deadlock avoidance by lock ordering

Transfer needs two account locks at once. Naïve "lock source then
destination" deadlocks instantly when A→B and B→A run concurrently.

| Strategy | Verdict |
|---|---|
| **Global lock ordering (chosen)** | Always lock the lower account id first. Cheap, deterministic, no deadlock possible because a cycle requires two threads to disagree on order. |
| `trylock` + back-off | Works, but livelock-prone and wastes work retrying. |
| One global "bank lock" | Trivially correct, but serialises every transaction in the bank — throughput collapses. |
| Banker's algorithm | Deadlock *avoidance* needs advance knowledge of maximum resource claims; enormous overkill for two fixed resources. |

Proven, not assumed: 300 transfers fired in both directions between the
same two accounts, no hang, balances exactly conserved.

### 4.10 Custom text protocol, not JSON/gRPC

**Why:** a serialization library would be exactly the kind of "library
function" the brief pushes back on, and hand-rolling the framing forces
the real lesson — TCP is a byte stream, so you need explicit message
boundaries (the 4-byte length prefix) and partial-read loops.

**Cost:** no escaping mechanism, so `|` and `;` must be banned from every
stored value; and a fixed 4096-byte client buffer becomes a hard response
size limit (§7 F7).

### 4.11 FNV-1a password hashing

**Honest answer:** this is the weakest part of the system and should be
presented as a deliberate, documented trade-off rather than defended as
secure. The brief says avoid libraries, and bcrypt/argon2 are libraries.
FNV-1a is a *non-cryptographic* hash: it is fast (the opposite of what you
want), **unsalted** (identical passwords produce identical stored hashes,
visible by inspecting `accounts.dat`) and 64-bit.

**What a good viva answer adds:** the correct fix that still respects the
constraint is key stretching — a per-account random salt plus thousands of
iterations of the existing hash, i.e. hand-rolling PBKDF2's structure. That
turns an offline attack from billions of guesses/second into thousands.

### 4.12 Loopback-only engine bind

The engine speaks an unauthenticated, unencrypted protocol, so it binds
`127.0.0.1` only. This keeps the custom engine protocol reachable only by
a gateway running on the same host.

---

## 5. Concurrency model in depth

### Lock inventory

| Lock | Type | Protects | Granularity |
|---|---|---|---|
| `g_locks[id]` | hand-rolled RW lock | one account record | per record (1000) |
| creation mutex | `pthread_mutex_t` | find-free-slot + claim | global (accounts only) |
| `g_staff_mutex` | mutex | all of `staff.dat` | per file |
| `g_loans_mutex` | mutex | all of `loans.dat` | per file |
| `g_feedback_mutex` | mutex | all of `feedback.dat` | per file |
| `g_conn_sem` | counting semaphore | connection slots | global (50) |
| session guard mutex | mutex | login-uniqueness table | global |
| auth guard mutex | mutex | lockout counters | global |
| `EngineConnection.lock` | Python mutex | one gateway↔engine socket | per session |

**Why accounts get per-record locks but loans/feedback/staff get one
mutex each:** contention. Two customers transacting on different accounts
must not block each other — that is the whole point of the joint-account
demo. Loan and feedback operations are rare and staff-driven, so a single
mutex is simpler and obviously correct. Deliberate asymmetry, worth
saying out loud.

### Lock ordering (the deadlock argument)

1. creation mutex → account RW lock (creation only)
2. account RW lock (lower id) → account RW lock (higher id) (transfer only)
3. Everything else takes **one** lock at a time.

No path takes a per-file mutex while holding an account lock, or vice
versa. Since every multi-lock path acquires in a globally consistent
order, no cycle can form, so deadlock is impossible.

**Deadlock I deliberately avoided:** `storage_for_each_feedback()` holds
`g_feedback_mutex` for the whole scan. Calling `storage_update_feedback()`
from inside the visit callback would try to re-acquire the same
non-recursive mutex and self-deadlock instantly. That is precisely why
"mark feedback as reviewed" is not implemented as part of listing — a good
answer to *"why is `Feedback.reviewed` never set?"*

### What is deliberately NOT protected

- Anything reading the `.dat` files **outside** the engine process.
- A second engine process (§7 F9).
- `txn_log.dat` across the process boundary — we rely on one atomic
  `write()` per line instead of a lock, because a pthread mutex cannot
  span the `fork()` (§7 F14).

---

## 6. ACID — what we actually guarantee

Asked directly about ACID, answer per property and be honest about D.

| Property | Status | Mechanism |
|---|---|---|
| **Atomicity** | Holds *in-process*; **not** crash-atomic | Both account locks held across validate-and-write; nothing is written if any check fails. But a crash **between** the two `pwrite`s destroys money (§7 F8). |
| **Consistency** | Holds | Debit and credit are the same amount by construction; writes only happen after both accounts are confirmed `ACTIVE` and funds sufficient. Measured: 300 concurrent bidirectional transfers conserved the total exactly. |
| **Isolation** | Holds (serialisable for these ops) | Both write locks are held for the *entire* critical section, so no deposit, withdrawal, transfer or balance read on either account can interleave. |
| **Durability** | Holds per-write | `fdatasync()` after every record write, so a committed write survives power loss — not merely sitting in the page cache. |

**The honest gap:** durability of *each write* is not the same as
atomicity of the *pair* of writes. We have no write-ahead log, so the
window in F8 is real. Saying this before the examiner finds it is much
stronger than being caught.

---

## 7. Failure catalogue

### Found by attacking the system, and fixed

#### F1 — A single rude client could kill the entire server (SIGPIPE) — **MEASURED, FIXED**

**Symptom:** the whole `bankserver` process (and its forked logger) died;
every logged-in user lost their session.

**Cause:** the engine never ignored `SIGPIPE`. When it `write()`s a reply
to a socket whose peer has gone away, the kernel raises `SIGPIPE`, whose
default action is *terminate the process*.

**Reproduction (deterministic):** saturate all 50 connection slots with
idle clients → a 51st client connects (lands in the backlog, not yet
accepted), sends a request, then gives up and closes → release the idle
clients → the server finally accepts the dead connection, reads the
request, writes the reply into a closed socket → dead.

Measured output before the fix:
```
bankserver pids at start: ['436', '444']
bystander logged in and holding an open session
saturated: 50 idle connections holding all 50 slots
51st client: request sent while queued in the backlog
51st client: gave up and closed
idle clients released -- server will now accept the dead connection
bankserver pids now: []
*** RESULT: SERVER PROCESS KILLED ***
```

**Blast radius:** total, instant outage triggered by one impatient client
— i.e. a trivially reachable denial of service, and easy to hit by
accident (a browser tab closed mid-request).

**Fix:** `signal(SIGPIPE, SIG_IGN)` in `main.c`. `write()` now returns
`-1/EPIPE`, which `write_full()` already propagates, so that one client's
thread exits cleanly and releases its session and semaphore slots.

**Verified:** re-ran the same reproduction — server survived, bystander
still served. Regression test added, and confirmed it *fails* when the fix
is reverted (66/67) and passes with it (67/67).

#### F2 — Concurrent account creation destroyed customer records — **MEASURED, FIXED**

**Symptom:** 40 accounts created concurrently produced only **28**
accounts. Twelve customers were told "account created, id = N" when their
record had been silently overwritten by someone else's.

**Cause:** classic TOCTOU. `handle_admin_add()` called
`storage_find_free_slot()` with **no lock held**, and only afterwards took
the per-record write lock. The per-record lock cannot help — you don't
know which record to lock until the search has already finished. Two
creators could therefore both pick the same free slot.

Measured before the fix:
```
account ids handed out : 40
distinct ids           : 28
ids given to >1 caller : ['1','2','5','12','13','14','15','16','17','18','19','22']
accounts actually in accounts.dat: 28
```

**Fix:** a dedicated creation mutex held across find-then-claim
(`lock_table_creation_lock()`), ordered creation-mutex → record lock so no
cycle is possible. Note the same bug class was already avoided in
`storage_add_staff/loan/feedback`, which do find-and-claim atomically
under their own mutex — accounts were the older code path that predated
that pattern.

**Verified after fix:** 40 handed out, 40 distinct, 40 present. Regression
test added.

#### F3 — Refreshing the page locked you out of your own account — **MEASURED, FIXED**

**Symptom:** press F5, and you cannot log back in for up to 15 minutes.

**Cause:** an interaction between two individually correct designs. React
keeps the role in memory only, so a refresh drops the UI back to the login
form — but the cookie still points at a live gateway session which still
holds the engine's one-session-per-user slot. Logging in again opened a
*second* engine connection, which the engine correctly refused.

**Fix (gateway, not engine — the engine's rule should stay strict):**
`/api/login` now retires any session the incoming cookie already owns
before opening a new one. Because `OP_LOGOUT` releases the identity
*before* replying, there is no race.

**Verified:** same-browser re-login → 200; a genuinely different browser
with no cookie → still correctly refused with 401 "already logged in from
another session".

### Known and open — with the fix for each

#### F4 — Deleted account ids are recycled, and transaction history bleeds — **MEASURED, OPEN**

Delete account 2, create a new account: it is handed id 2 again. Because
`txn_log.dat` is filtered by account id only, the **new** customer sees the
**previous** customer's transactions.

Measured — the new owner's history showed the deleted owner's deposit:
```
new owner's history contains the PREVIOUS owner's transaction: True
2026-09-19 06:38:11|2|ghost|DEPOSIT|123.45|623.45|-
```
**Severity:** privacy breach plus incorrect statements. **Fix:** never
recycle ids — allocate monotonically from a persisted counter; or add a
generation/epoch number to the id and record it in every log line.

#### F5 — 50 idle sockets starve the whole bank — **MEASURED, OPEN**

`MAX_CONNECTIONS` is 50 and there is **no idle timeout** on engine client
connections. Fifty connections that never send a byte hold every slot
indefinitely; the 51st client connects (TCP handshake completes via the
backlog) but never receives a response.

Measured: *"51st client: connect() succeeded but NO RESPONSE after 3.0s."*

**Fix:** `SO_RCVTIMEO` on accepted sockets, or a per-connection idle
deadline reaped by a watchdog thread; and/or per-IP connection caps.

#### F6 — Kill the logger and audit records vanish silently — **MEASURED, OPEN**

With the forked logger killed, transactions keep succeeding but nothing
is written to `txn_log.dat`, and nothing alarms or restarts it.

Measured: balance moved to 133.33 while `txn_log.dat` stayed at 1144 bytes.

**Cause:** `msgsnd(..., IPC_NOWAIT)` — deliberately non-blocking so a slow
logger can't stall a customer's transaction — silently discards on
failure, and no process supervises the logger.

**Fix:** have the parent `waitpid`/re-`fork()` the logger; count and expose
dropped messages; and for a real bank, write the audit record as part of
the transaction's journal rather than fire-and-forget.

#### F7 — `bankclient` breaks on large listings — **MEASURED, OPEN**

The CLI's receive buffer is `MAX_PAYLOAD` (4096 B) and `recv_frame()`
rejects any larger frame. Measured: 33 accounts → 1978 bytes, so the CLI
breaks at roughly **62 accounts**. The Python gateway is unaffected — it
reads exactly `length` bytes with no fixed buffer.

**Fix:** paginate list opcodes (offset/limit), or make the CLI allocate
dynamically from the length prefix like the gateway does.

#### F8 — A crash mid-transfer destroys money — **ANALYSED, OPEN** (most important open issue)

```c
int rc1 = storage_write_account(&from_acc);   // debit  + fdatasync
int rc2 = storage_write_account(&to_acc);     // credit + fdatasync
```
If the process dies between these, the **debit is durable and the credit
never happened** — money disappears. The locks cannot help: they are
in-memory and die with the process. `fdatasync` arguably makes it worse by
guaranteeing the debit is on disk first. If `rc2` merely *fails*, we
report an error but do not roll back.

**Fix:** a write-ahead log. Append `{txn_id, from, to, amount, PREPARED}`
and `fsync` it; perform both writes; append `COMMITTED`. On startup, replay
`PREPARED`-without-`COMMITTED` entries. This is exactly what real databases
do, and saying so is the strongest possible answer here.

#### F9 — The locks only work because there is exactly one engine process — **ANALYSED, OPEN**

Every lock is a pthread object in one address space. Start a second
`bankserver` on the same data directory and the two processes share no
locks at all — concurrent read-modify-write would corrupt balances. Also
why `fork()`-per-client was rejected (§4.5).

**Fix:** `fcntl()` byte-range locks (kernel-level, cross-process) layered
on top; or mutexes in shared memory with `PTHREAD_PROCESS_SHARED`; or an
exclusive lock file so a second instance refuses to start.

#### F10 — Lockout and session state reset on restart — **ANALYSED, OPEN**

`auth_guard` and `session_guard` are in-memory. Restarting the engine
clears all brute-force lockouts and all "logged in" flags, so an attacker
who can crash the server resets the rate limiter (which F1 made easy until
it was fixed). **Fix:** persist lockout state, or accept it and rate-limit
at the gateway too.

#### F11 — Money is stored as `double` — **ANALYSED, OPEN**

Binary floating point cannot represent 0.10 exactly, so repeated
operations accumulate error. It is currently *masked*: the wire format is
`%.2f` and the tests compare with a `1e-9` epsilon, so a 10 × 0.10 deposit
still prints `1.00`. It is latent, not absent.

**Fix:** store integer paise/cents (`long long`) and format on display —
the standard answer, and the one examiners look for.

#### F12 — Everything is a linear scan — **ANALYSED, OPEN**

`storage_find_free_slot()` is O(n) `pread`s; `ADMIN_LIST` locks and reads
all 1000 slots; `TXN_HISTORY` reads the **entire** log file into memory and
keeps only the last 20 lines; `txn_log.dat` never rotates. At scale this is
both a performance wall and a memory-based DoS.

**Fix:** in-memory free-list and index, per-account transaction files or an
index, log rotation, reading the log backwards in chunks.

#### F13 — Coarse locking on loans/feedback/staff — **ANALYSED, OPEN**

One mutex per file, and `storage_for_each_*` holds it for the whole scan,
so two employees listing loans serialise completely. Acceptable at this
scale, deliberate, but a real limit. **Fix:** per-record locks, or
read-mostly snapshots.

#### F14 — `txn_log.dat` cross-process safety rests on a platform assumption — **ANALYSED, OPEN**

The logger process appends while the main process reads. We rely on "one
`write()` per line is atomic with respect to a concurrent reader". POSIX
only guarantees that for pipes ≤ `PIPE_BUF`; on Linux it holds in practice
because the inode is locked during a buffered write. This is why the
logger builds the whole line (text + `\n`) and issues **one** `write()` —
the original two-call version could expose a line without its newline.
**Fix:** `O_APPEND` + a length-prefixed binary record format, or a real
lock, if strict portability mattered.

#### F15 — The System V queue leaks on `SIGKILL` — **ANALYSED, OPEN**

`ipc_queue_destroy()` runs on clean shutdown and via the `SIGTERM`
handler, but `SIGKILL` cannot be caught, so the queue survives the process
and lingers system-wide (`ipcs -q`, remove with `ipcrm`). **Fix:** POSIX
message queues, or a `socketpair()` between parent and child, which the
kernel reclaims automatically.

#### F16–F17 — Transport and password weaknesses — **ANALYSED, OPEN**

No TLS anywhere: passwords cross the engine socket in plaintext (mitigated
only by loopback-only binding) and the browser talks plain HTTP. Password
hashing is unsalted FNV-1a (§4.11). Both are documented trade-offs of the
"no external libraries" constraint, not oversights — but say so explicitly.

#### F18–F19 — Functional gaps worth admitting — **ANALYSED, OPEN**

Approving a loan sets its status but **does not disburse any money** into
the account — the workflow is approval-only. And `Feedback.reviewed` is
written at creation but never updated, for the deadlock reason in §5.

#### F20 — The gateway caps the web app at ~50 concurrent users — **ANALYSED, OPEN**

The gateway holds one engine socket per logged-in browser session, and the
engine allows 50 connections total. The 51st web user is simply not served.
**Fix:** a connection pool with per-request re-authentication, or raise the
cap, or multiplex several sessions over one engine connection.

---

## 8. Stress-test evidence

Quote these numbers directly.

| Test | Result |
|---|---|
| 500 concurrent `+1` deposits, both joint owners at once | Exactly the expected balance; **zero lost updates** |
| 300 concurrent transfers, both directions between the same two accounts | No deadlock; total balance **conserved exactly** (2000.00 → 2000.00) |
| 200 mixed concurrent deposits/withdrawals (net zero) | Final balance exactly unchanged |
| Two simultaneous withdrawals of 700 from a balance of 1000 | Exactly **one** succeeds; no overdraft |
| Two simultaneous withdrawals of 500 from 1000 (sum == balance) | Both succeed, balance lands exactly on 0.00 |
| `nan` / `inf` / `-0` as amounts | All rejected, balance untouched |
| 5 wrong passwords, then the correct one | Correct password still refused — lockout works |
| Same identity logging in twice concurrently | Second refused; after logout, immediately allowed |
| 30 concurrent account creations | 30 distinct ids, 30 accounts (after F2 fix) |
| Rude client disconnecting mid-request | Server survives (after F1 fix) |
| **Full suite** | **68/68 passing** |

---

## 9. Likely viva questions

**"Why not just use `fcntl()` for locking?"** Because that is the OS's own
locking service — the requirement is to implement the mechanism. Then
volunteer the trade-off: our lock is process-local, so it only works
because a single engine process owns the files (F9).

**"Your lock is in memory — what if the server crashes?"** All lock state
dies with the process, which is fine for locks (nobody holds them any
more) but not for the transfer window in F8, and it resets brute-force
lockouts (F10).

**"Is your transfer really atomic?"** In-process, yes — both locks held
across validate-and-write. Crash-atomic, no: there is no write-ahead log,
so a crash between the two writes loses money. Here is where a WAL would go.

**"How do you avoid deadlock?"** Global lock ordering by account id, so a
cycle cannot form. Proven with 300 bidirectional concurrent transfers.
Alternatives (trylock/back-off, one global lock, banker's algorithm) and
why each is worse — §4.9.

**"Why a semaphore and not a mutex for connections?"** Counting resource
(50 interchangeable slots) versus mutual exclusion (one holder). Then
mention the side effect: no idle timeout turns it into a DoS (F5).

**"Why is `double` wrong for money?"** 0.1 is not representable in binary
floating point; errors accumulate and equality comparisons break. Should
be integer paise. Ours is masked by `%.2f` formatting, not fixed (F11).

**"What happens if two people withdraw at the same time?"** The write lock
serialises them; the second re-reads the balance *after* the first
commits, so the overdraft check is made on fresh data. Measured: exactly
one of two 700-withdrawals from 1000 succeeds.

**"Where does your system break?"** Lead with F5, F8 and F4 — a specific,
measured answer with a named fix reads far better than "it's fine".

**"What would you change first?"** Write-ahead log for transfers (F8),
integer money (F11), idle timeouts (F5), non-recycled ids (F4).

---

## 10. Improvement roadmap, in priority order

1. **Write-ahead log for transfers** — closes the only way the system can
   actually lose money (F8).
2. **Integer paise instead of `double`** — removes a whole class of latent
   correctness bugs (F11).
3. **Idle timeouts + per-IP caps on engine connections** — closes the
   measured DoS (F5).
4. **Stop recycling account ids** — closes the measured privacy leak (F4).
5. **Supervise the logger; count dropped audit records** (F6).
6. **Salted, iterated password hashing** — still hand-rolled, so the
   "no library" constraint holds (§4.11).
7. **Index/free-list instead of linear scans; rotate the transaction log** (F12).
8. **Pagination for list opcodes** — fixes the CLI limit (F7).
9. **TLS**, if the constraint is ever relaxed (F16).
