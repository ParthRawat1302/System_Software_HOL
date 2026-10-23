#!/usr/bin/env python3
"""
Automated smoke/regression test suite for the C banking engine.

Runs entirely against engine/bankserver over its raw socket protocol --
no gateway, no browser. Must be run where the engine can actually run
(Linux/WSL), since it uses fork()/pthread/sockets.

Usage (from repo root, inside WSL):
    python3 tests/test_engine.py

It builds the engine if needed, wipes engine/data for a clean slate,
starts bankserver as a subprocess, runs a series of protocol-level
assertions (including a concurrency stress test on a joint account's
hand-rolled RW-lock), and shuts the server down cleanly at the end.
Exits 0 if every test passes, 1 otherwise.
"""

import os
import socket
import struct
import subprocess
import sys
import threading
import time
import ctypes

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ENGINE_DIR = os.path.join(REPO_ROOT, "engine")
SERVER_BIN = os.path.join(ENGINE_DIR, "bankserver")
HOST = "127.0.0.1"
PORT = 9090

OP_LOGIN = 1
OP_DEPOSIT = 2
OP_WITHDRAW = 3
OP_BALANCE = 4
OP_CHANGE_PW = 5
OP_VIEW_DETAILS = 6
OP_LOGOUT = 7
OP_ADMIN_ADD = 8
OP_ADMIN_DELETE = 9
OP_ADMIN_MODIFY = 10
OP_ADMIN_SEARCH = 11
OP_ADMIN_LIST = 12
OP_TXN_HISTORY = 13
OP_TRANSFER = 14
OP_LOAN_APPLY = 15
OP_LOAN_MY_LIST = 16
OP_LOAN_ASSIGNED_LIST = 17
OP_LOAN_DECIDE = 18
OP_LOAN_ASSIGN = 19
OP_FEEDBACK_ADD = 20
OP_FEEDBACK_LIST = 21
OP_ACCOUNT_SET_STATUS = 22
OP_STAFF_ADD = 23
OP_STAFF_MODIFY = 24
OP_STAFF_LIST = 25
OP_LOAN_PENDING_LIST = 26
OP_LOAN_DECISION_HISTORY = 27


class Client:
    def __init__(self):
        self.sock = socket.create_connection((HOST, PORT), timeout=5)

    def request(self, opcode, payload=""):
        body = bytes([opcode]) + payload.encode("utf-8")
        self.sock.sendall(struct.pack(">I", len(body)) + body)
        header = self._recv_exact(4)
        total_len = struct.unpack(">I", header)[0]
        data = self._recv_exact(total_len)
        return data[1:].decode("utf-8")

    def _recv_exact(self, n):
        data = b""
        while len(data) < n:
            chunk = self.sock.recv(n - len(data))
            if not chunk:
                raise ConnectionError("server closed connection")
            data += chunk
        return data

    def close(self):
        self.sock.close()


def parse_status(raw):
    status, _, rest = raw.partition("|")
    return status == "OK", rest


def parse_kv(rest):
    out = {}
    for field in rest.split("|"):
        k, _, v = field.partition("=")
        out[k] = v
    return out


# ---------- test harness ----------

results = []


def check(name, condition, detail=""):
    status = "PASS" if condition else "FAIL"
    results.append((name, condition, detail))
    print(f"[{status}] {name}" + (f" -- {detail}" if detail and not condition else ""))
    return condition


def build_engine():
    subprocess.run(["make", "-C", ENGINE_DIR], check=True)


def wait_for_port(timeout=5):
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            s = socket.create_connection((HOST, PORT), timeout=0.5)
            s.close()
            return True
        except OSError:
            time.sleep(0.1)
    return False


def start_server():
    data_dir = os.path.join(ENGINE_DIR, "data")
    subprocess.run(["rm", "-rf", data_dir])
    os.makedirs(data_dir, exist_ok=True)
    class LegacyStaff(ctypes.Structure):
        _fields_ = [
            ("staff_id", ctypes.c_int),
            ("role", ctypes.c_int),
            ("status", ctypes.c_int),
            ("name", ctypes.c_char * 64),
            ("pw_hash", ctypes.c_ulong),
        ]

    legacy_manager = LegacyStaff()
    legacy_manager.staff_id = 2
    legacy_manager.role = 2
    legacy_manager.status = 1
    legacy_manager.name = b"manager"
    legacy_manager.pw_hash = 1469598103934665603
    for byte in b"pass123":
        legacy_manager.pw_hash = (
            (legacy_manager.pw_hash ^ byte) * 1099511628211
        ) & ((1 << (ctypes.sizeof(ctypes.c_ulong) * 8)) - 1)
    with open(os.path.join(data_dir, "staff.dat"), "wb") as staff_file:
        staff_file.seek(2 * ctypes.sizeof(LegacyStaff))
        staff_file.write(bytes(legacy_manager))

    proc = subprocess.Popen([SERVER_BIN], cwd=ENGINE_DIR, stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT, text=True)
    if not wait_for_port():
        proc.terminate()
        raise RuntimeError("server never opened port " + str(PORT))
    return proc


def stop_server(proc):
    proc.terminate()  # SIGTERM -> shutdown_handler kills the forked logger too
    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait()


# ---------- tests ----------

def run_tests():
    c = Client()

    ok, rest = parse_status(c.request(OP_LOGIN, "ADMIN|1|wrongpassword"))
    check("admin login rejects wrong password", not ok, rest)

    ok, rest = parse_status(c.request(OP_LOGIN, "ADMIN|1|pass123"))
    check("admin login with seeded default admin", ok, rest)

    demo_user = Client()
    ok, rest = parse_status(demo_user.request(OP_LOGIN, "USER|1|pass123"))
    check("seeded demo customer can log in", ok, rest)
    ok, rest = parse_status(demo_user.request(OP_BALANCE))
    check("seeded demo customer starts with zero balance", ok and rest == "0.00", rest)
    demo_user.request(OP_LOGOUT)
    demo_user.close()

    demo_staff = Client()
    ok, rest = parse_status(demo_staff.request(OP_LOGIN, "EMPLOYEE|1|pass123"))
    check("seeded demo employee can log in", ok and "employee" in rest, rest)
    demo_manager = Client()
    ok, rest = parse_status(demo_manager.request(OP_LOGIN, "MANAGER|1|pass123"))
    check("seeded demo manager has independent ID 1 while employee ID 1 is active",
          ok and "manager" in rest, rest)
    demo_staff.request(OP_LOGOUT)
    demo_manager.request(OP_LOGOUT)
    demo_staff.close()
    demo_manager.close()

    ok, rest = parse_status(c.request(OP_ADMIN_ADD, "JOINT|alice|pw1|bob|pw2|100.00"))
    check("admin creates joint account", ok, rest)
    joint_id = rest if ok else None

    ok, rest = parse_status(c.request(OP_ADMIN_ADD, "NORMAL|carol|pw3|-|-|50.00"))
    check("admin creates normal account", ok, rest)
    normal_id = rest if ok else None

    ok, rest = parse_status(c.request(OP_ADMIN_LIST))
    entries = [parse_kv(e) for e in rest.split(";")] if ok and rest else []
    ids_seen = {e.get("id") for e in entries}
    check("admin list includes both new accounts", ok and joint_id in ids_seen and normal_id in ids_seen,
          f"rest={rest!r}")

    ok, rest = parse_status(c.request(OP_ADMIN_SEARCH, normal_id))
    check("admin search finds normal account", ok and parse_kv(rest).get("owner1") == "carol", rest)

    c.request(OP_LOGOUT)
    c.close()

    # --- normal user flows ---
    u = Client()
    ok, rest = parse_status(u.request(OP_LOGIN, f"USER|{normal_id}|pw3"))
    check("normal user login", ok, rest)

    ok, rest = parse_status(u.request(OP_DEPOSIT, "25"))
    check("deposit increases balance", ok and rest == "75.00", rest)

    ok, rest = parse_status(u.request(OP_WITHDRAW, "1000"))
    check("withdraw beyond balance is rejected", not ok and "insufficient" in rest, rest)

    ok, rest = parse_status(u.request(OP_WITHDRAW, "20"))
    check("withdraw within balance succeeds", ok and rest == "55.00", rest)

    ok, rest = parse_status(u.request(OP_CHANGE_PW, "wrongold|newpw"))
    check("password change rejects wrong old password", not ok, rest)

    ok, rest = parse_status(u.request(OP_CHANGE_PW, "pw3|newpw3"))
    check("password change with correct old password", ok, rest)

    u.request(OP_LOGOUT)
    u.close()

    u2 = Client()
    ok, rest = parse_status(u2.request(OP_LOGIN, f"USER|{normal_id}|pw3"))
    check("old password no longer works after change", not ok, rest)
    ok, rest = parse_status(u2.request(OP_LOGIN, f"USER|{normal_id}|newpw3"))
    check("new password works after change", ok, rest)
    u2.request(OP_LOGOUT)
    u2.close()

    # --- joint account: independent owner logins share one balance ---
    j1 = Client()
    ok, _ = parse_status(j1.request(OP_LOGIN, f"USER|{joint_id}|pw1"))
    check("joint owner1 login", ok)
    ok, rest = parse_status(j1.request(OP_DEPOSIT, "50"))
    check("owner1 deposit reflected", ok and rest == "150.00", rest)
    j1.request(OP_LOGOUT)
    j1.close()

    j2 = Client()
    ok, _ = parse_status(j2.request(OP_LOGIN, f"USER|{joint_id}|pw2"))
    check("joint owner2 login (independent password) sees shared balance", ok)
    ok, rest = parse_status(j2.request(OP_BALANCE))
    check("owner2 sees owner1's deposit", ok and rest == "150.00", rest)
    ok, rest = parse_status(j2.request(OP_WITHDRAW, "30"))
    check("owner2 withdraw from shared balance", ok and rest == "120.00", rest)
    j2.request(OP_LOGOUT)
    j2.close()

    # --- transaction history, with owner attribution on the joint account ---
    h1 = Client()
    parse_status(h1.request(OP_LOGIN, f"USER|{joint_id}|pw1"))
    ok, rest = parse_status(h1.request(OP_TXN_HISTORY, ""))
    entries = rest.split(";") if rest else []
    check("transaction history has both joint-account entries", ok and len(entries) == 2, rest)
    newest, oldest = (entries + ["", ""])[:2]
    check("newest entry is owner2's withdraw, correctly attributed", ok and "|bob|WITHDRAW|30.00|120.00" in newest, newest)
    check("oldest entry is owner1's deposit, correctly attributed", ok and "|alice|DEPOSIT|50.00|150.00" in oldest, oldest)

    ok, rest = parse_status(h1.request(OP_TXN_HISTORY, str(normal_id)))
    check("a USER's history request ignores any account id in the payload (always their own account)",
          ok and len(rest.split(";")) == 2, rest)
    h1.request(OP_LOGOUT)
    h1.close()

    h2 = Client()
    parse_status(h2.request(OP_LOGIN, "ADMIN|1|pass123"))
    ok, rest = parse_status(h2.request(OP_TXN_HISTORY, str(joint_id)))
    check("admin can view any account's transaction history by id", ok and len(rest.split(";")) == 2, rest)
    h2.request(OP_LOGOUT)
    h2.close()

    # --- non-admin session cannot use admin opcodes ---
    u3 = Client()
    parse_status(u3.request(OP_LOGIN, f"USER|{joint_id}|pw1"))
    ok, rest = parse_status(u3.request(OP_ADMIN_DELETE, normal_id))
    check("normal session is refused admin opcodes", not ok, rest)
    u3.request(OP_LOGOUT)
    u3.close()

    # --- concurrency: hand-rolled RW-lock must serialize read-modify-write ---
    N = 250
    errors = []

    def worker(password):
        try:
            wc = Client()
            ok, _ = parse_status(wc.request(OP_LOGIN, f"USER|{joint_id}|{password}"))
            if not ok:
                errors.append("worker login failed")
                return
            for _ in range(N):
                ok, rest = parse_status(wc.request(OP_DEPOSIT, "1"))
                if not ok:
                    errors.append(f"deposit failed: {rest}")
            wc.request(OP_LOGOUT)
            wc.close()
        except Exception as e:  # noqa: BLE001
            errors.append(str(e))

    before = Client()
    parse_status(before.request(OP_LOGIN, f"USER|{joint_id}|pw1"))
    _, before_balance = parse_status(before.request(OP_BALANCE))
    before.request(OP_LOGOUT)
    before.close()

    t1 = threading.Thread(target=worker, args=("pw1",))
    t2 = threading.Thread(target=worker, args=("pw2",))
    t1.start(); t2.start(); t1.join(); t2.join()

    after = Client()
    parse_status(after.request(OP_LOGIN, f"USER|{joint_id}|pw1"))
    _, after_balance = parse_status(after.request(OP_BALANCE))
    after.request(OP_LOGOUT)
    after.close()

    expected = float(before_balance) + 2 * N
    check(
        f"custom RW-lock: {2*N} concurrent deposits from both joint holders lose none",
        not errors and abs(float(after_balance) - expected) < 1e-9,
        f"errors={errors[:3]} before={before_balance} after={after_balance} expected={expected:.2f}",
    )

    # --- admin delete ---
    admin2 = Client()
    parse_status(admin2.request(OP_LOGIN, "ADMIN|1|pass123"))
    ok, rest = parse_status(admin2.request(OP_ADMIN_DELETE, normal_id))
    check("admin deletes account", ok, rest)

    # --- security regression: NaN/Infinity amount injection (fixed) ---
    ok, rest = parse_status(admin2.request(OP_ADMIN_ADD, "NORMAL|nanvictim|pwN|-|-|100.00"))
    nan_victim_id = rest if ok else None
    check("setup account for NaN-injection test", ok, rest)

    victim = Client()
    parse_status(victim.request(OP_LOGIN, f"USER|{nan_victim_id}|pwN"))
    ok, rest = parse_status(victim.request(OP_DEPOSIT, "nan"))
    check("deposit rejects 'nan' as an amount (was: silently corrupted balance to NaN)", not ok, rest)
    ok, rest = parse_status(victim.request(OP_DEPOSIT, "inf"))
    check("deposit rejects 'inf' as an amount", not ok, rest)
    ok, rest = parse_status(victim.request(OP_WITHDRAW, "nan"))
    check("withdraw rejects 'nan' as an amount", not ok, rest)
    ok, rest = parse_status(victim.request(OP_BALANCE))
    check("balance is untouched by the rejected NaN/Infinity attempts", ok and rest == "100.00", rest)
    victim.request(OP_LOGOUT)
    victim.close()

    # --- security regression: '|'/';' injection into stored text fields (fixed) ---
    ok, rest = parse_status(admin2.request(OP_ADMIN_ADD, "NORMAL|evil|owner1_pw|-|-|10.00"))
    check("setup account for injection test", ok, rest)
    injected_id = rest

    ok, rest = parse_status(admin2.request(OP_ADMIN_MODIFY, f"{injected_id}|owner1_name|hacker;fake_entry=1"))
    check("admin_modify rejects ';' in a name (was: could forge extra ADMIN_LIST entries)", not ok, rest)

    ok, rest = parse_status(admin2.request(OP_ADMIN_ADD, "JOINT|a|pw|bad|name|extra|50.00"))
    check("admin_add rejects a payload with an extra '|' (was: silently shifted every later field)", not ok, rest)

    admin2.request(OP_LOGOUT)
    admin2.close()

    # --- security regression: brute-force lockout (new) ---
    admin3 = Client()
    parse_status(admin3.request(OP_LOGIN, "ADMIN|1|pass123"))
    ok, rest = parse_status(admin3.request(OP_ADMIN_ADD, "NORMAL|lockouttest|correctpw|-|-|0.00"))
    lockout_id = rest if ok else None
    admin3.request(OP_LOGOUT)
    admin3.close()
    check("setup throwaway account for lockout test", ok, rest)

    lc = Client()
    for _ in range(5):
        parse_status(lc.request(OP_LOGIN, f"USER|{lockout_id}|wrongpassword"))
    ok, rest = parse_status(lc.request(OP_LOGIN, f"USER|{lockout_id}|correctpw"))
    check(
        "account is locked out after 5 failed attempts, even with the correct password next",
        not ok and "too many failed attempts" in rest,
        rest,
    )
    lc.close()

    u4 = Client()
    ok, rest = parse_status(u4.request(OP_LOGIN, f"USER|{normal_id}|newpw3"))
    check("deleted account can no longer log in", not ok, rest)
    u4.close()


def run_role_and_acid_tests():
    """Covers the CS-513 structure: Employee/Manager roles, one-session-
    per-user, and the ACID-property funds transfer -- including a
    concurrent bidirectional stress test proving the deadlock-avoidance
    lock ordering actually works, not just that it compiles."""

    admin = Client()
    parse_status(admin.request(OP_LOGIN, "ADMIN|1|pass123"))

    ok, rest = parse_status(admin.request(OP_STAFF_ADD, "EMPLOYEE|bob_emp|emp123"))
    check("admin adds an Employee", ok, rest)
    emp_id = rest

    ok, rest = parse_status(admin.request(OP_STAFF_ADD, "MANAGER|carol_mgr|mgr123"))
    check("admin adds a Manager", ok, rest)
    mgr_id = rest

    ok, rest = parse_status(admin.request(OP_ADMIN_ADD, "NORMAL|alice|alicepw|-|-|1000.00"))
    check("admin creates customer alice", ok, rest)
    alice_id = rest

    ok, rest = parse_status(admin.request(OP_ADMIN_ADD, "NORMAL|dan|danpw|-|-|1000.00"))
    check("admin creates customer dan", ok, rest)
    dan_id = rest

    ok, rest = parse_status(admin.request(OP_STAFF_LIST))
    check("staff list shows both new staff members", ok and f"id={emp_id}|" in rest and f"id={mgr_id}|" in rest, rest)
    admin.request(OP_LOGOUT)
    admin.close()

    # --- Employee: can manage customer accounts, cannot manage staff ---
    emp = Client()
    ok, rest = parse_status(emp.request(OP_LOGIN, f"STAFF|{emp_id}|emp123"))
    check("employee login", ok, rest)

    ok, rest = parse_status(emp.request(OP_ADMIN_ADD, "NORMAL|erin|erinpw|-|-|500.00"))
    check("employee can add a customer account", ok, rest)

    ok, rest = parse_status(emp.request(OP_STAFF_ADD, "EMPLOYEE|nope|nope"))
    check("employee is refused staff management (admin-only)", not ok, rest)
    emp.request(OP_LOGOUT)
    emp.close()

    # --- Manager: activate/deactivate customer accounts ---
    mgr = Client()
    parse_status(mgr.request(OP_LOGIN, f"MANAGER|{mgr_id}|mgr123"))

    ok, rest = parse_status(mgr.request(OP_ACCOUNT_SET_STATUS, f"{dan_id}|INACTIVE"))
    check("manager deactivates a customer account", ok, rest)
    mgr.request(OP_LOGOUT)
    mgr.close()

    blocked = Client()
    ok, rest = parse_status(blocked.request(OP_LOGIN, f"USER|{dan_id}|danpw"))
    check("a deactivated account cannot log in", not ok, rest)
    blocked.close()

    mgr2 = Client()
    parse_status(mgr2.request(OP_LOGIN, f"MANAGER|{mgr_id}|mgr123"))
    ok, rest = parse_status(mgr2.request(OP_ACCOUNT_SET_STATUS, f"{dan_id}|ACTIVE"))
    check("manager reactivates the account", ok, rest)
    mgr2.request(OP_LOGOUT)
    mgr2.close()

    # --- one session per user ---
    alice1 = Client()
    ok, rest = parse_status(alice1.request(OP_LOGIN, f"USER|{alice_id}|alicepw"))
    check("alice's first session logs in", ok, rest)

    alice2 = Client()
    ok, rest = parse_status(alice2.request(OP_LOGIN, f"USER|{alice_id}|alicepw"))
    check("a second concurrent session for the same identity is rejected",
          not ok and "already logged in" in rest, rest)
    alice2.close()

    alice1.request(OP_LOGOUT)
    alice1.close()

    alice3 = Client()
    ok, rest = parse_status(alice3.request(OP_LOGIN, f"USER|{alice_id}|alicepw"))
    check("logging out releases the identity immediately for a new login", ok, rest)

    # --- ACID transfer: single-request correctness ---
    ok, rest = parse_status(alice3.request(OP_TRANSFER, f"{dan_id}|200"))
    check("alice transfers 200 to dan", ok and rest == "800.00", rest)

    dan = Client()
    parse_status(dan.request(OP_LOGIN, f"USER|{dan_id}|danpw"))
    ok, rest = parse_status(dan.request(OP_BALANCE))
    check("dan received the transferred 200", ok and rest == "1200.00", rest)

    ok, rest = parse_status(alice3.request(OP_TRANSFER, f"{alice_id}|10"))
    check("transfer to the same account is rejected", not ok, rest)

    ok, rest = parse_status(alice3.request(OP_TRANSFER, f"{dan_id}|999999"))
    check("over-balance transfer is rejected (and atomically -- nothing partially applied)",
          not ok and "insufficient" in rest, rest)

    alice3.request(OP_LOGOUT)
    alice3.close()
    dan.request(OP_LOGOUT)
    dan.close()

    # --- ACID transfer: concurrent bidirectional stress test ---
    def get_balance(acc_id, pw):
        c = Client()
        parse_status(c.request(OP_LOGIN, f"USER|{acc_id}|{pw}"))
        _, bal = parse_status(c.request(OP_BALANCE))
        c.request(OP_LOGOUT)
        c.close()
        return float(bal)

    before_total = get_balance(alice_id, "alicepw") + get_balance(dan_id, "danpw")

    n = 150
    errors = []

    def transfer_worker(from_id, from_pw, to_id, count):
        try:
            wc = Client()
            ok, rest = parse_status(wc.request(OP_LOGIN, f"USER|{from_id}|{from_pw}"))
            if not ok:
                errors.append(f"login failed: {rest}")
                return
            for _ in range(count):
                ok, rest = parse_status(wc.request(OP_TRANSFER, f"{to_id}|1"))
                if not ok:
                    errors.append(f"transfer failed: {rest}")
            wc.request(OP_LOGOUT)
            wc.close()
        except Exception as e:  # noqa: BLE001
            errors.append(str(e))

    t1 = threading.Thread(target=transfer_worker, args=(alice_id, "alicepw", dan_id, n))
    t2 = threading.Thread(target=transfer_worker, args=(dan_id, "danpw", alice_id, n))
    t1.start(); t2.start()
    t1.join(timeout=30); t2.join(timeout=30)

    check("bidirectional concurrent transfers complete without deadlock (lock ordering works)",
          not t1.is_alive() and not t2.is_alive())

    after_total = get_balance(alice_id, "alicepw") + get_balance(dan_id, "danpw")
    check(
        f"{2*n} concurrent transfers in both directions: total balance conserved exactly (ACID)",
        not errors and abs(after_total - before_total) < 1e-9,
        f"errors={errors[:5]} before_total={before_total:.2f} after_total={after_total:.2f}",
    )

    # --- loans ---
    balance_before_loan = get_balance(alice_id, "alicepw")
    alice4 = Client()
    parse_status(alice4.request(OP_LOGIN, f"USER|{alice_id}|alicepw"))
    ok, rest = parse_status(alice4.request(OP_LOAN_APPLY, "5000|home renovation"))
    check("customer applies for a loan", ok, rest)
    loan_id = rest

    ok, rest = parse_status(alice4.request(OP_LOAN_MY_LIST))
    check("customer sees their own loan application", ok and f"id={loan_id}|" in rest, rest)
    alice4.request(OP_LOGOUT)
    alice4.close()

    mgr3 = Client()
    parse_status(mgr3.request(OP_LOGIN, f"MANAGER|{mgr_id}|mgr123"))
    ok, rest = parse_status(mgr3.request(OP_LOAN_ASSIGN, f"{loan_id}|{emp_id}"))
    check("manager assigns the loan to an employee", ok, rest)
    mgr3.request(OP_LOGOUT)
    mgr3.close()

    emp2 = Client()
    parse_status(emp2.request(OP_LOGIN, f"STAFF|{emp_id}|emp123"))
    ok, rest = parse_status(emp2.request(OP_LOAN_ASSIGNED_LIST))
    check("employee sees the loan assigned to them", ok and f"id={loan_id}|" in rest, rest)

    ok, rest = parse_status(emp2.request(OP_LOAN_DECIDE, f"{loan_id}|APPROVE"))
    check("employee approves the assigned loan", ok, rest)

    ok, rest = parse_status(emp2.request(OP_LOAN_DECISION_HISTORY))
    check("employee can view loan decision history", ok and f"id={loan_id}|" in rest and "status=APPROVED" in rest and f"decided_by={emp_id}" in rest, rest)

    ok, rest = parse_status(emp2.request(OP_LOAN_DECIDE, f"{loan_id}|REJECT"))
    check("an already-decided loan cannot be decided again", not ok, rest)
    emp2.request(OP_LOGOUT)
    emp2.close()

    balance_after_loan = get_balance(alice_id, "alicepw")
    check("approved loan is credited to the applicant account",
          abs(balance_after_loan - balance_before_loan - 5000.0) < 1e-9,
          f"before={balance_before_loan:.2f} after={balance_after_loan:.2f}")
    alice_history = Client()
    parse_status(alice_history.request(OP_LOGIN, f"USER|{alice_id}|alicepw"))
    ok, rest = parse_status(alice_history.request(OP_TXN_HISTORY, ""))
    check("loan disbursement appears in customer transaction history",
          ok and "LOAN_DISBURSEMENT" in rest and "5000.00" in rest, rest)
    alice_history.request(OP_LOGOUT)
    alice_history.close()

    mgr_loan_history = Client()
    parse_status(mgr_loan_history.request(OP_LOGIN, f"MANAGER|{mgr_id}|mgr123"))
    ok, rest = parse_status(mgr_loan_history.request(OP_LOAN_DECISION_HISTORY))
    check("manager can view employee loan decisions",
          ok and f"id={loan_id}|" in rest and "status=APPROVED" in rest and f"decided_by={emp_id}" in rest, rest)
    mgr_loan_history.request(OP_LOGOUT)
    mgr_loan_history.close()

    # --- feedback ---
    dan2 = Client()
    parse_status(dan2.request(OP_LOGIN, f"USER|{dan_id}|danpw"))
    ok, rest = parse_status(dan2.request(OP_FEEDBACK_ADD, "great service overall"))
    check("customer submits feedback", ok, rest)
    dan2.request(OP_LOGOUT)
    dan2.close()

    mgr4 = Client()
    parse_status(mgr4.request(OP_LOGIN, f"MANAGER|{mgr_id}|mgr123"))
    ok, rest = parse_status(mgr4.request(OP_FEEDBACK_LIST))
    check("manager reviews submitted feedback", ok and "great service overall" in rest, rest)
    mgr4.request(OP_LOGOUT)
    mgr4.close()

    role_admin = Client()
    parse_status(role_admin.request(OP_LOGIN, "ADMIN|1|pass123"))
    ok, rest = parse_status(role_admin.request(OP_LOAN_DECISION_HISTORY))
    check("administrator can view loan decision history",
          ok and f"id={loan_id}|" in rest and "status=APPROVED" in rest and f"decided_by={emp_id}" in rest, rest)
    ok, rest = parse_status(
        role_admin.request(OP_STAFF_MODIFY, f"EMPLOYEE|{emp_id}|role|MANAGER")
    )
    check("admin changes an Employee into a Manager across ID namespaces", ok, rest)
    ok, rest = parse_status(role_admin.request(OP_STAFF_LIST))
    staff_entries = [parse_kv(entry) for entry in rest.split(";")] if ok and rest else []
    promoted = next(
        (
            entry for entry in staff_entries
            if entry.get("name") == "bob_emp" and entry.get("role") == "MANAGER"
        ),
        None,
    )
    check(
        "promoted staff member appears under the Manager role",
        promoted is not None and promoted.get("role") == "MANAGER",
        rest,
    )
    role_admin.request(OP_LOGOUT)
    role_admin.close()

    old_employee = Client()
    ok, rest = parse_status(old_employee.request(OP_LOGIN, f"EMPLOYEE|{emp_id}|emp123"))
    check("promoted Employee credentials no longer log in as Employee", not ok, rest)
    old_employee.close()
    if promoted:
        promoted_manager = Client()
        ok, rest = parse_status(
            promoted_manager.request(OP_LOGIN, f"MANAGER|{promoted['id']}|emp123")
        )
        check("promoted staff member logs in as Manager", ok and "manager" in rest, rest)
        if ok:
            promoted_manager.request(OP_LOGOUT)
        promoted_manager.close()

    # --- regression: a client that disconnects before reading its reply
    # must not kill the server (SIGPIPE). Reproduces the exact ordering
    # that did kill it: saturate every connection slot, queue one more
    # client that sends a request then gives up, then free the slots so
    # the server finally answers into a dead socket. ---
    bystander = Client()
    ok, _ = parse_status(bystander.request(OP_LOGIN, "ADMIN|1|pass123"))

    idle = [socket.create_connection((HOST, PORT), timeout=5) for _ in range(50)]
    doomed = socket.create_connection((HOST, PORT), timeout=5)
    body = bytes([OP_BALANCE])
    doomed.sendall(struct.pack(">I", len(body)) + body)
    time.sleep(0.5)
    doomed.close()
    for s in idle:
        s.close()
    time.sleep(1.5)

    survived = True
    try:
        ok, rest = parse_status(bystander.request(OP_ADMIN_LIST))
        survived = ok
    except (ConnectionError, OSError):
        survived = False
    finally:
        try:
            bystander.request(OP_LOGOUT)
            bystander.close()
        except (ConnectionError, OSError):
            pass

    check("a client vanishing before reading its reply does not kill the server (SIGPIPE)", survived)

    # --- regression: concurrent account creation must not hand the same
    # free slot to two creators. find_free_slot() + write used to run
    # unsynchronised, so two simultaneous creations could both claim the
    # same id and the second silently overwrote the first (measured: 40
    # creations produced only 28 accounts). ---
    setup = Client()
    parse_status(setup.request(OP_LOGIN, "ADMIN|1|pass123"))
    _, race_emp1 = parse_status(setup.request(OP_STAFF_ADD, "EMPLOYEE|racer1|rpw1"))
    _, race_emp2 = parse_status(setup.request(OP_STAFF_ADD, "EMPLOYEE|racer2|rpw2"))
    setup.request(OP_LOGOUT)
    setup.close()

    per_thread = 15
    created_ids = []
    id_lock = threading.Lock()
    start_line = threading.Barrier(2, timeout=30)

    def create_worker(staff_id, pw, tag):
        try:
            wc = Client()
            ok_, _rest = parse_status(wc.request(OP_LOGIN, f"STAFF|{staff_id}|{pw}"))
            if not ok_:
                start_line.abort()
                return
            start_line.wait()
            mine = []
            for i in range(per_thread):
                ok_, rest_ = parse_status(wc.request(OP_ADMIN_ADD, f"NORMAL|{tag}{i}|pw|-|-|1.00"))
                if ok_:
                    mine.append(rest_)
            with id_lock:
                created_ids.extend(mine)
            wc.request(OP_LOGOUT)
            wc.close()
        except Exception:  # noqa: BLE001
            try:
                start_line.abort()
            except Exception:  # noqa: BLE001
                pass

    c1 = threading.Thread(target=create_worker, args=(race_emp1, "rpw1", "ra"))
    c2 = threading.Thread(target=create_worker, args=(race_emp2, "rpw2", "rb"))
    c1.start(); c2.start()
    c1.join(60); c2.join(60)

    check(
        f"{2*per_thread} concurrent account creations produce {2*per_thread} distinct ids (no slot collision)",
        len(created_ids) == 2 * per_thread and len(set(created_ids)) == len(created_ids),
        f"handed out {len(created_ids)}, distinct {len(set(created_ids))}",
    )


def main():
    build_engine()
    proc = start_server()
    try:
        run_tests()
        run_role_and_acid_tests()
    finally:
        stop_server(proc)

    failed = [r for r in results if not r[1]]
    print(f"\n{len(results) - len(failed)}/{len(results)} passed")
    if failed:
        print("FAILURES:")
        for name, _, detail in failed:
            print(f"  - {name}: {detail}")
        sys.exit(1)
    sys.exit(0)


if __name__ == "__main__":
    main()
