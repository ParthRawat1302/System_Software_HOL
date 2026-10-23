"""
Thin client for the C engine's wire protocol.

This mirrors engine/src/protocol.c exactly: a 4-byte big-endian length
prefix (opcode byte + payload), then the payload itself. No serialization
library is used on either side -- this is the same hand-rolled framing the
C server speaks, just read/written with Python's raw `socket` module
(itself a direct wrapper over the socket()/connect()/send()/recv() syscalls).

The gateway holds NO banking logic. Every method here does exactly one
thing: send a request frame, read a response frame, return it. All
decisions (is the password right, is there enough balance, who holds the
write lock) are made inside the C engine.
"""

import socket
import struct
import threading

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

HEADER_LEN = 4


class EngineError(Exception):
    pass


class EngineConnection:
    """One TCP connection to the C engine, corresponding to exactly one
    logged-in session there. Not safe for concurrent use from two threads
    at once -- callers must hold `self.lock` for the duration of a request
    (a single browser session issuing overlapping requests is not a
    supported case for this project)."""

    def __init__(self, host: str, port: int):
        self.sock = socket.create_connection((host, port), timeout=5)
        self.lock = threading.Lock()

    def _send_frame(self, opcode: int, payload: str) -> None:
        body = bytes([opcode]) + payload.encode("utf-8")
        header = struct.pack(">I", len(body))
        self.sock.sendall(header + body)

    def _recv_exact(self, n: int) -> bytes:
        data = b""
        while len(data) < n:
            chunk = self.sock.recv(n - len(data))
            if not chunk:
                raise EngineError("engine closed the connection")
            data += chunk
        return data

    def _recv_frame(self) -> str:
        header = self._recv_exact(HEADER_LEN)
        total_len = struct.unpack(">I", header)[0]
        body = self._recv_exact(total_len)
        return body[1:].decode("utf-8")  # drop echoed opcode byte

    def request(self, opcode: int, payload: str = "") -> str:
        with self.lock:
            self._send_frame(opcode, payload)
            return self._recv_frame()

    def close(self) -> None:
        try:
            self.sock.close()
        except OSError:
            pass


def parse_status(raw: str) -> tuple[bool, str]:
    """Engine responses are 'OK|<rest>' or 'ERR|<reason>'."""
    status, _, rest = raw.partition("|")
    return status == "OK", rest
