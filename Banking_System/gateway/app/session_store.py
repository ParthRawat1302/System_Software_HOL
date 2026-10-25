"""In-memory map from a web session token to its live C-engine connection.

The C engine enforces idle connection expiry. This gateway store handles
explicit logout, replacement logins, and cleanup when the gateway shuts down.
"""

import secrets
import threading
from dataclasses import dataclass

from .engine_client import EngineConnection, EngineError, OP_LOGOUT

_lock = threading.Lock()


@dataclass
class _Entry:
    conn: EngineConnection


_sessions: dict[str, _Entry] = {}


def create_session(conn: EngineConnection) -> str:
    token = secrets.token_hex(16)
    with _lock:
        _sessions[token] = _Entry(conn=conn)
    return token


def get_session(token: str | None) -> EngineConnection | None:
    if not token:
        return None
    with _lock:
        entry = _sessions.get(token)
        if entry is None:
            return None
        return entry.conn


def drop_session(token: str | None) -> None:
    if not token:
        return
    with _lock:
        entry = _sessions.pop(token, None)
    if entry:
        entry.conn.close()


def drop_connection(conn: EngineConnection) -> None:
    """Remove and close every gateway token mapped to a failed engine socket."""
    with _lock:
        tokens = [token for token, entry in _sessions.items() if entry.conn is conn]
        entries = [_sessions.pop(token) for token in tokens]
    for entry in entries:
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
    except (EngineError, OSError):
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
        except (EngineError, OSError):
            pass
        finally:
            entry.conn.close()
