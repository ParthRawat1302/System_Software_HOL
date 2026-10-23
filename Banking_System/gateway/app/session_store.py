"""In-memory map from a web session token to the open EngineConnection that
represents that browser's logged-in session inside the C engine. This is
the only "state" the gateway keeps -- it is a live connection handle, not a
copy of any account data.

A session is explicitly closed on /api/logout, but a browser tab can also
just be closed or crash without ever calling logout -- with nothing else,
that would leak an open engine-side connection (and its logged-in session)
forever. `sweep_idle()` is run on a timer from a background thread started
in main.py's startup hook to close and drop any session that hasn't been
touched in IDLE_TIMEOUT_SECONDS.
"""

import secrets
import threading
import time
from dataclasses import dataclass

from .engine_client import EngineConnection, EngineError, OP_LOGOUT

IDLE_TIMEOUT_SECONDS = 15 * 60

_lock = threading.Lock()


@dataclass
class _Entry:
    conn: EngineConnection
    last_used: float


_sessions: dict[str, _Entry] = {}


def create_session(conn: EngineConnection) -> str:
    token = secrets.token_hex(16)
    with _lock:
        _sessions[token] = _Entry(conn=conn, last_used=time.monotonic())
    return token


def get_session(token: str | None) -> EngineConnection | None:
    if not token:
        return None
    with _lock:
        entry = _sessions.get(token)
        if entry is None:
            return None
        entry.last_used = time.monotonic()
        return entry.conn


def drop_session(token: str | None) -> None:
    if not token:
        return
    with _lock:
        entry = _sessions.pop(token, None)
    if entry:
        entry.conn.close()


def retire_session(token: str | None) -> None:
    """Synchronously release an existing engine login, if this browser has one.

    This is used before issuing a replacement login and on normal logout.
    Closing a socket eventually releases the engine session too, but sending
    OP_LOGOUT first removes the identity guard before a new LOGIN is tried.
    """
    if not token:
        return
    with _lock:
        entry = _sessions.pop(token, None)
    if not entry:
        return
    try:
        entry.conn.request(OP_LOGOUT, "")
    except EngineError:
        pass
    finally:
        entry.conn.close()


def close_all_sessions() -> None:
    """Close every live engine socket during a gateway shutdown/reload."""
    with _lock:
        entries = list(_sessions.values())
        _sessions.clear()
    for entry in entries:
        try:
            entry.conn.request(OP_LOGOUT, "")
        except EngineError:
            pass
        finally:
            entry.conn.close()


def sweep_idle(idle_timeout: float = IDLE_TIMEOUT_SECONDS) -> int:
    """Closes and drops every session untouched for longer than
    idle_timeout seconds. Returns how many were reaped."""
    now = time.monotonic()
    with _lock:
        expired = [tok for tok, e in _sessions.items() if now - e.last_used > idle_timeout]
        entries = [_sessions.pop(tok) for tok in expired]
    for entry in entries:
        entry.conn.close()
    return len(entries)


def start_reaper(interval_seconds: float = 60, idle_timeout: float = IDLE_TIMEOUT_SECONDS) -> None:
    """Starts a daemon thread that calls sweep_idle() on a timer for the
    life of the process. Safe to call once at app startup."""

    def _loop():
        while True:
            time.sleep(interval_seconds)
            sweep_idle(idle_timeout)

    threading.Thread(target=_loop, daemon=True, name="session-reaper").start()
