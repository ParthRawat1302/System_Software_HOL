"""
REST/JSON gateway in front of the C banking engine.

This process holds no account data and makes no banking decisions. Every
endpoint below does the same three things: look up the caller's engine
connection from their session cookie, forward one opcode/payload to the C
engine over its custom socket protocol, and translate the text response
back into JSON for React. All login validation, balance math, locking, and
admin authorization happen inside the C engine (see engine/src/server.c).
"""

import os

from fastapi import FastAPI, HTTPException, Request, Response
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel

from . import engine_client as proto
from . import session_store
from .engine_client import EngineConnection, EngineError, parse_status

ENGINE_HOST = os.environ.get("ENGINE_HOST", "127.0.0.1")
ENGINE_PORT = int(os.environ.get("ENGINE_PORT", "9090"))
COOKIE_NAME = "session_token"
SESSION_HEADER = "X-Session-Token"

app = FastAPI(title="Banking Gateway")

_default_origins = ["http://localhost:5173", "http://localhost:5174", "http://127.0.0.1:5173", "http://127.0.0.1:5174"]
_extra_origin = os.environ.get("FRONTEND_ORIGIN")

app.add_middleware(
    CORSMiddleware,
    allow_origins=_default_origins + ([_extra_origin] if _extra_origin else []),
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)


@app.on_event("shutdown")
def _close_sessions_on_shutdown() -> None:
    # Particularly important with `uvicorn --reload`: the session store is
    # in-memory, but the engine sockets are real external resources.
    session_store.close_all_sessions()


def require_session(request: Request) -> EngineConnection:
    # A cookie is shared by every tab for an origin, which made a Manager
    # login in one tab replace a Customer's live engine session in another.
    # The React client now sends a per-tab sessionStorage token in this
    # header. Keep the cookie as a fallback for older clients/CLI bridges.
    token = request.headers.get(SESSION_HEADER) or request.cookies.get(COOKIE_NAME)
    conn = session_store.get_session(token)
    if conn is None:
        raise HTTPException(status_code=401, detail="not logged in")
    return conn


def engine_call(conn: EngineConnection, opcode: int, payload: str = "") -> dict:
    try:
        raw = conn.request(opcode, payload)
    except (EngineError, OSError):
        session_store.drop_connection(conn)
        raise HTTPException(
            status_code=401,
            detail="session expired or engine connection lost; please log in again",
        )
    ok, rest = parse_status(raw)
    if not ok:
        raise HTTPException(status_code=400, detail=rest)
    return {"ok": True, "data": rest}


def parse_kv_details(rest: str) -> dict:
    details = {}
    for field in rest.split("|"):
        key, _, value = field.partition("=")
        details[key] = value
    return details


def parse_history(rest: str) -> list[dict]:
    # Each entry is "timestamp|account_id|owner|operation|amount|balance|
    # counterparty" (positional, not key=value -- see docs/protocol.md for
    # OP_TXN_HISTORY). counterparty is "-" for deposit/withdraw, or the
    # other account's id for a transfer leg.
    if not rest:
        return []
    entries = []
    for entry in rest.split(";"):
        parts = entry.split("|")
        if len(parts) != 7:
            continue
        entries.append(
            {
                "timestamp": parts[0],
                "account_id": parts[1],
                "owner": parts[2],
                "operation": parts[3],
                "amount": parts[4],
                "balance": parts[5],
                "counterparty": parts[6],
            }
        )
    return entries


def parse_list(rest: str) -> list[dict]:
    """Generic parser for the ';'-joined, 'key=value|key=value' list
    responses shared by admin/staff/loan listings."""
    return [parse_kv_details(entry) for entry in rest.split(";")] if rest else []


# ---------- auth ----------

class LoginRequest(BaseModel):
    kind: str  # "USER", "EMPLOYEE", "MANAGER", or "ADMIN"
    id: int
    password: str


@app.post("/api/login")
def login(body: LoginRequest, request: Request, response: Response):
    # Only retire the caller's *tab-scoped* token. Do not retire a cookie
    # here: cookies are shared across tabs and might belong to a different
    # currently active role.
    session_store.retire_session(request.headers.get(SESSION_HEADER))
    conn = EngineConnection(ENGINE_HOST, ENGINE_PORT)
    kind = body.kind.strip().upper()
    payload = f"{kind}|{body.id}|{body.password}"
    try:
        raw = conn.request(proto.OP_LOGIN, payload)
    except EngineError:
        conn.close()
        raise HTTPException(status_code=502, detail="engine unreachable")

    ok, rest = parse_status(raw)
    if not ok:
        conn.close()
        raise HTTPException(status_code=401, detail=rest)

    token = session_store.create_session(conn)

    staff_role = None
    if kind in ("EMPLOYEE", "MANAGER", "STAFF"):
        staff_role = "MANAGER" if "manager" in rest else "EMPLOYEE"

    return {
        "ok": True,
        "message": rest,
        "kind": kind,
        "staff_role": staff_role,
        "session_token": token,
    }


@app.post("/api/logout")
def logout(request: Request, response: Response):
    session_store.retire_session(request.headers.get(SESSION_HEADER) or request.cookies.get(COOKIE_NAME))
    response.delete_cookie(COOKIE_NAME)
    return {"ok": True}


# ---------- normal / joint user operations ----------

class AmountRequest(BaseModel):
    amount: float


@app.post("/api/deposit")
def deposit(body: AmountRequest, request: Request):
    conn = require_session(request)
    result = engine_call(conn, proto.OP_DEPOSIT, f"{body.amount:.2f}")
    return {"ok": True, "balance": float(result["data"])}


@app.post("/api/withdraw")
def withdraw(body: AmountRequest, request: Request):
    conn = require_session(request)
    result = engine_call(conn, proto.OP_WITHDRAW, f"{body.amount:.2f}")
    return {"ok": True, "balance": float(result["data"])}


@app.get("/api/balance")
def balance(request: Request):
    conn = require_session(request)
    result = engine_call(conn, proto.OP_BALANCE)
    return {"ok": True, "balance": float(result["data"])}


class ChangePasswordRequest(BaseModel):
    old_password: str
    new_password: str


@app.post("/api/password")
def change_password(body: ChangePasswordRequest, request: Request):
    conn = require_session(request)
    payload = f"{body.old_password}|{body.new_password}"
    result = engine_call(conn, proto.OP_CHANGE_PW, payload)
    return {"ok": True, "message": result["data"]}


@app.get("/api/details")
def view_details(request: Request):
    conn = require_session(request)
    result = engine_call(conn, proto.OP_VIEW_DETAILS)
    return {"ok": True, "details": parse_kv_details(result["data"])}


@app.get("/api/history")
def history(request: Request):
    conn = require_session(request)
    result = engine_call(conn, proto.OP_TXN_HISTORY)
    return {"ok": True, "transactions": parse_history(result["data"])}


class TransferRequest(BaseModel):
    to_account_id: int
    amount: float


@app.post("/api/transfer")
def transfer(body: TransferRequest, request: Request):
    conn = require_session(request)
    payload = f"{body.to_account_id}|{body.amount:.2f}"
    result = engine_call(conn, proto.OP_TRANSFER, payload)
    return {"ok": True, "balance": float(result["data"])}


class LoanApplyRequest(BaseModel):
    amount: float
    purpose: str


@app.post("/api/loans")
def loan_apply(body: LoanApplyRequest, request: Request):
    conn = require_session(request)
    payload = f"{body.amount:.2f}|{body.purpose}"
    result = engine_call(conn, proto.OP_LOAN_APPLY, payload)
    return {"ok": True, "loan_id": int(result["data"])}


@app.get("/api/loans/mine")
def loan_my_list(request: Request):
    conn = require_session(request)
    result = engine_call(conn, proto.OP_LOAN_MY_LIST)
    return {"ok": True, "loans": parse_list(result["data"])}


@app.get("/api/loans/assigned")
def loan_assigned_list(request: Request):
    conn = require_session(request)
    result = engine_call(conn, proto.OP_LOAN_ASSIGNED_LIST)
    return {"ok": True, "loans": parse_list(result["data"])}


@app.get("/api/loans/pending")
def loan_pending_list(request: Request):
    conn = require_session(request)
    result = engine_call(conn, proto.OP_LOAN_PENDING_LIST)
    return {"ok": True, "loans": parse_list(result["data"])}


@app.get("/api/loans/decisions")
def loan_decision_history(request: Request):
    conn = require_session(request)
    result = engine_call(conn, proto.OP_LOAN_DECISION_HISTORY)
    return {"ok": True, "loans": parse_list(result["data"])}


class LoanDecisionRequest(BaseModel):
    decision: str  # "APPROVE" or "REJECT"


@app.post("/api/loans/{loan_id}/decision")
def loan_decide(loan_id: int, body: LoanDecisionRequest, request: Request):
    conn = require_session(request)
    payload = f"{loan_id}|{body.decision}"
    result = engine_call(conn, proto.OP_LOAN_DECIDE, payload)
    return {"ok": True, "message": result["data"]}


class LoanAssignRequest(BaseModel):
    employee_id: int


@app.post("/api/loans/{loan_id}/assign")
def loan_assign(loan_id: int, body: LoanAssignRequest, request: Request):
    conn = require_session(request)
    payload = f"{loan_id}|{body.employee_id}"
    result = engine_call(conn, proto.OP_LOAN_ASSIGN, payload)
    return {"ok": True, "message": result["data"]}


class FeedbackRequest(BaseModel):
    text: str


@app.post("/api/feedback")
def feedback_add(body: FeedbackRequest, request: Request):
    conn = require_session(request)
    result = engine_call(conn, proto.OP_FEEDBACK_ADD, body.text)
    return {"ok": True, "feedback_id": int(result["data"])}


@app.get("/api/feedback")
def feedback_list(request: Request):
    conn = require_session(request)
    result = engine_call(conn, proto.OP_FEEDBACK_LIST)
    return {"ok": True, "feedback": parse_list(result["data"])}


class AccountStatusRequest(BaseModel):
    status: str  # "ACTIVE" or "INACTIVE"


@app.put("/api/manager/accounts/{account_id}/status")
def account_set_status(account_id: int, body: AccountStatusRequest, request: Request):
    conn = require_session(request)
    payload = f"{account_id}|{body.status}"
    result = engine_call(conn, proto.OP_ACCOUNT_SET_STATUS, payload)
    return {"ok": True, "message": result["data"]}


# ---------- staff (Employee / Manager) management -- Administrator only ----------

class StaffAddRequest(BaseModel):
    role: str  # "EMPLOYEE" or "MANAGER"
    name: str
    password: str


@app.post("/api/staff")
def staff_add(body: StaffAddRequest, request: Request):
    conn = require_session(request)
    payload = f"{body.role}|{body.name}|{body.password}"
    result = engine_call(conn, proto.OP_STAFF_ADD, payload)
    return {"ok": True, "staff_id": int(result["data"])}


class StaffModifyRequest(BaseModel):
    field: str  # "role" | "name" | "status"
    value: str


@app.put("/api/staff/{role}/{staff_id}")
def staff_modify(role: str, staff_id: int, body: StaffModifyRequest, request: Request):
    conn = require_session(request)
    payload = f"{role}|{staff_id}|{body.field}|{body.value}"
    result = engine_call(conn, proto.OP_STAFF_MODIFY, payload)
    return {"ok": True, "message": result["data"]}


@app.get("/api/staff")
def staff_list(request: Request):
    conn = require_session(request)
    result = engine_call(conn, proto.OP_STAFF_LIST)
    return {"ok": True, "staff": parse_list(result["data"])}


# ---------- admin operations ----------

class AdminAddRequest(BaseModel):
    role: str  # "NORMAL" or "JOINT"
    owner1_name: str
    owner1_password: str
    owner2_name: str = "-"
    owner2_password: str = "-"
    initial_balance: float = 0.0


@app.post("/api/admin/accounts")
def admin_add(body: AdminAddRequest, request: Request):
    conn = require_session(request)
    payload = (
        f"{body.role}|{body.owner1_name}|{body.owner1_password}|"
        f"{body.owner2_name}|{body.owner2_password}|{body.initial_balance:.2f}"
    )
    result = engine_call(conn, proto.OP_ADMIN_ADD, payload)
    return {"ok": True, "account_id": int(result["data"])}


@app.get("/api/admin/accounts")
def admin_list(request: Request):
    conn = require_session(request)
    result = engine_call(conn, proto.OP_ADMIN_LIST)
    return {"ok": True, "accounts": parse_list(result["data"])}


@app.delete("/api/admin/accounts/{account_id}")
def admin_delete(account_id: int, request: Request):
    conn = require_session(request)
    result = engine_call(conn, proto.OP_ADMIN_DELETE, str(account_id))
    return {"ok": True, "message": result["data"]}


@app.delete("/api/customers/{account_id}")
def customer_close(account_id: int, request: Request):
    """Close an account for an authorised Employee or Administrator.

    The engine implements this as a soft delete, preserving audit records
    while preventing all future customer logins and transactions.
    """
    conn = require_session(request)
    result = engine_call(conn, proto.OP_ADMIN_DELETE, str(account_id))
    return {"ok": True, "message": result["data"]}


class AdminModifyRequest(BaseModel):
    field: str  # "balance" | "owner1_name" | "owner2_name"
    value: str


@app.put("/api/admin/accounts/{account_id}")
def admin_modify(account_id: int, body: AdminModifyRequest, request: Request):
    conn = require_session(request)
    payload = f"{account_id}|{body.field}|{body.value}"
    result = engine_call(conn, proto.OP_ADMIN_MODIFY, payload)
    return {"ok": True, "message": result["data"]}


@app.get("/api/admin/accounts/{account_id}")
def admin_search(account_id: int, request: Request):
    conn = require_session(request)
    result = engine_call(conn, proto.OP_ADMIN_SEARCH, str(account_id))
    return {"ok": True, "details": parse_kv_details(result["data"])}


@app.get("/api/admin/accounts/{account_id}/history")
def admin_history(account_id: int, request: Request):
    conn = require_session(request)
    result = engine_call(conn, proto.OP_TXN_HISTORY, str(account_id))
    return {"ok": True, "transactions": parse_history(result["data"])}
