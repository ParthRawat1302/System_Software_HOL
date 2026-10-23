# Engine wire protocol

Plain, hand-rolled framing over TCP -- no RPC/serialization library on
either the C or the Python side.

```
[4-byte big-endian length][1-byte opcode][payload bytes...]
```

`length` counts the opcode byte plus the payload. The payload is a plain
`|`-delimited ASCII string. Responses are always `OK|<data>` or
`ERR|<reason>`, echoed on the same opcode.

| Opcode | Value | Request payload | Response (`OK|...`) |
|---|---|---|---|
| `LOGIN` | 1 | `KIND|id|password` (`KIND` = `USER`, `EMPLOYEE`, `MANAGER`, or `ADMIN`; legacy `STAFF` is accepted as `EMPLOYEE`) | `welcome` / `welcome employee` / `welcome manager` / `welcome admin` |
| `DEPOSIT` | 2 | `amount` | new balance |
| `WITHDRAW` | 3 | `amount` | new balance |
| `BALANCE` | 4 | *(empty)* | balance |
| `CHANGE_PW` | 5 | `old_password|new_password` | `password changed` (works for `USER`, `STAFF`, and `ADMIN` sessions alike) |
| `VIEW_DETAILS` | 6 | *(empty)* | `id=..|role=..|status=..|owner1=..[|owner2=..]|balance=..` |
| `LOGOUT` | 7 | *(empty)* | `bye`, then the connection is closed |
| `ADMIN_ADD` | 8 | `role|owner1_name|owner1_pw|owner2_name|owner2_pw|initial_balance` | new `account_id` |
| `ADMIN_DELETE` | 9 | `account_id` | `deleted` (soft-close: account login and transactions are disabled, while records and audit history remain) |
| `ADMIN_MODIFY` | 10 | `account_id|field|value` (`field` in `balance`, `owner1_name`, `owner2_name`) | `modified` |
| `ADMIN_SEARCH` | 11 | `account_id` | same format as `VIEW_DETAILS` |
| `ADMIN_LIST` | 12 | *(empty)* | active accounts joined by `;`, each in the same `id=..\|role=..\|...` format as `VIEW_DETAILS` |
| `TXN_HISTORY` | 13 | *(empty for `USER`; `account_id` for staff/admin)* | last 20 transactions (newest first) joined by `;`, each `timestamp\|account_id\|owner\|op\|amount\|balance\|counterparty` |
| `TRANSFER` | 14 | `to_account_id|amount` | sender's new balance |
| `LOAN_APPLY` | 15 | `amount|purpose` | new `loan_id` |
| `LOAN_MY_LIST` | 16 | *(empty)* | caller's own loan applications, `;`-joined `id=..\|account=..\|amount=..\|purpose=..\|status=..\|assigned=..` |
| `LOAN_ASSIGNED_LIST` | 17 | *(empty)* | loans assigned to this Employee (or, for Admin, every assigned loan) |
| `LOAN_DECIDE` | 18 | `loan_id|APPROVE` or `REJECT` | `approved` / `rejected` |
| `LOAN_ASSIGN` | 19 | `loan_id|employee_id` | `assigned` |
| `FEEDBACK_ADD` | 20 | free-text feedback | new `feedback_id` |
| `FEEDBACK_LIST` | 21 | *(empty)* | all feedback, `;`-joined `id=..\|account=..\|text=..` |
| `ACCOUNT_SET_STATUS` | 22 | `account_id|ACTIVE` or `INACTIVE` | `updated` |
| `STAFF_ADD` | 23 | `role|name|password` (`role` = `EMPLOYEE` or `MANAGER`) | new `staff_id` |
| `STAFF_MODIFY` | 24 | `role|staff_id|field|value` (`role` = `EMPLOYEE` or `MANAGER`; `field` in `role`, `name`, `status`) | `modified` |
| `STAFF_LIST` | 25 | *(empty)* | all staff, `;`-joined `id=..\|role=..\|status=..\|name=..` |
| `LOAN_PENDING_LIST` | 26 | *(empty)* | unassigned pending loans, `;`-joined in the standard loan format |
| `LOAN_DECISION_HISTORY` | 27 | *(empty)* | all non-empty loans with current status and assigned employee |

`owner2_*` fields are ignored server-side unless `role == JOINT`.

`TXN_HISTORY` behaves differently depending on the caller's role: a
`USER` session always gets *their own* account's history regardless of
whatever is in the payload (there's no way for a logged-in user to
request another account's history through this opcode); an Employee or
Administrator session must pass the target `account_id` explicitly
(this is the "passbook" feature). `owner` is the actual holder's name at
the time the transaction happened -- e.g. on a joint account, one entry
says `alice` and another says `bob`, which is what lets a UI show which
of the two account holders made each transaction. `counterparty` is `-`
for a plain deposit/withdraw, or the other account's id for a transfer
leg (`TRANSFER_OUT`/`TRANSFER_IN`).

## Roles and who can call what

`USER` (Customer), `EMPLOYEE`, `MANAGER`, and `ADMIN` (Administrator,
superset access over everything Employee/Manager can do) are enforced
server-side on every opcode (`engine/src/server.c`'s `is_admin`/
`is_manager`/`is_employee`/`is_staff_any` helpers), never trusted from the
client. Employee and Manager IDs are separate namespaces, so both may
have an ID 1:

| Opcode(s) | Customer | Employee | Manager | Admin |
|---|---|---|---|---|
| Deposit/Withdraw/Transfer/Balance/Loan apply/Feedback add | own account only | -- | -- | -- |
| `ADMIN_ADD`/`ADMIN_MODIFY`/`ADMIN_SEARCH`/`ADMIN_LIST` | -- | yes | search/list only | yes |
| `ADMIN_DELETE` (close customer account) | -- | yes | -- | yes |
| `TXN_HISTORY` (any account) | -- | yes | -- | yes |
| `LOAN_ASSIGNED_LIST`/`LOAN_DECIDE` | -- | own assigned loans | -- | any loan |
| `LOAN_ASSIGN`/`FEEDBACK_LIST`/`ACCOUNT_SET_STATUS` | -- | -- | yes | yes |
| `LOAN_PENDING_LIST` | -- | -- | yes | yes |
| `LOAN_DECISION_HISTORY` | -- | -- | yes | yes |
| `STAFF_ADD`/`STAFF_MODIFY` | -- | -- | -- | yes |
| `STAFF_LIST` | -- | -- | yes | yes |

Every account-touching opcode acquires the hand-rolled RWLock for that
`account_id` before reading/writing `accounts.dat` (read lock for
`BALANCE`/`VIEW_DETAILS`/`ADMIN_SEARCH`, write lock for everything that
mutates a record) -- see `engine/src/rwlock.c`.

## Protocol invariants (enforced server-side, see `engine/src/server.c`)

- **No field value may contain `|` or `;`.** `|` is the request field
  delimiter and `;` is the `ADMIN_LIST` response's account separator; a
  value containing either would silently shift later fields or forge
  fake list entries. This applies to passwords, owner names, and the
  login password -- there is no escaping mechanism, so these characters
  are simply rejected outright at account-creation, password-change, and
  login time.
- **Fixed-field opcodes (`LOGIN`, `CHANGE_PW`, `ADMIN_ADD`, `ADMIN_MODIFY`)
  reject a payload whose `|` count doesn't exactly match the expected
  field count**, checked *before* splitting. `split_fields()` on its own
  folds any extra `|` into the last field rather than erroring, which
  combined with the "no `|` in values" rule above makes an exact count
  match the only way the split is unambiguous.
- **Amounts are parsed with a strict decimal parser, not `atof()`.**
  `atof("nan")` returns NaN, and `NaN <= 0` is always false in C, so a
  naive `if (amount <= 0) reject` check does not catch it -- an engine
  built that way will happily write a NaN balance to disk. `parse_amount()`
  (see `engine/src/util.c`) explicitly rejects non-finite results and
  unparsed trailing input.
- **Login lockout**: after 5 consecutive failed attempts against the same
  `(kind, id)`, further attempts are refused for 30 seconds *without
  checking the password at all* -- see `engine/src/auth_guard.c`.
- **The engine binds to `127.0.0.1` only.** It is meant to be reached by
  the gateway on the same host, not directly over the network -- the
  protocol here has no transport encryption or authentication of its own.
- **Financial history is durable.** Deposit, withdrawal, and transfer
  records are written to `ledger.dat` as part of the same WAL transaction
  as their account balance changes. `TXN_HISTORY` filters this shared ledger
  by account ID and returns the latest committed entries only.
- **One session per identity.** Every role's login attempt is checked
  against `engine/src/common/session_guard.c` before it's granted; a second
  concurrent login for the *same* identity is refused with
  `ERR|already logged in from another session`. On a joint account, the
  two owners are two different identities (keyed by `account_id` *and*
  which owner slot matched), so they can still be logged in
  simultaneously -- only the same owner logging in twice at once is
  blocked. `LOGOUT` releases the slot *before* replying, so a client that
  receives `OK|bye` is guaranteed the identity is immediately free to log
  in again elsewhere. Employee and Manager session-guard keys are distinct
  even when both IDs are 1. A dropped connection (no `LOGOUT` sent) also
  releases the slot, from the same code path in `client_thread`'s cleanup.
- **At most `MAX_CONNECTIONS` (50) clients are served at once**, enforced
  by a POSIX counting semaphore (`sem_wait`/`sem_post` around
  `accept()`/thread-exit in `engine/src/server.c`) -- a connection beyond
  that limit simply waits in the kernel's `listen()` backlog until a
  slot frees up, rather than spawning an unbounded number of threads.
- **`TRANSFER` is atomic across two accounts with deadlock-free
  locking.** Both accounts' write locks are acquired in ascending
  account-id order (never in request order), so two transfers running in
  opposite directions at the same time can never deadlock -- proven in
  `tests/test_engine.py` with hundreds of transfers fired concurrently in
  both directions. See `docs/design.md` for the full ACID discussion.
