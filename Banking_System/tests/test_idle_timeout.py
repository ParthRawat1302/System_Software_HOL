#!/usr/bin/env python3
"""Verify idle engine connections expire and release the login guard.

Builds and runs an isolated temporary engine copy; project data is untouched.
Run from the repository root with:
    python3 tests/test_idle_timeout.py
"""

import os
import shutil
import socket
import struct
import subprocess
import tempfile
import time
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parent.parent
ENGINE_SOURCE = REPO_ROOT / "engine"
IDLE_TIMEOUT_SECONDS = 2


def choose_port():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


def recv_exact(sock, size):
    data = bytearray()
    while len(data) < size:
        chunk = sock.recv(size - len(data))
        if not chunk:
            raise ConnectionError("engine closed the connection")
        data.extend(chunk)
    return bytes(data)


def login(sock):
    payload = b"ADMIN|1|pass123"
    body = bytes([1]) + payload
    sock.sendall(struct.pack(">I", len(body)) + body)
    length = struct.unpack(">I", recv_exact(sock, 4))[0]
    return recv_exact(sock, length)[1:].decode("utf-8")


def prepare_engine(destination, port):
    shutil.copytree(
        ENGINE_SOURCE,
        destination,
        ignore=shutil.ignore_patterns(
            "data", "*.o", "bankserver", "bankclient", "__pycache__"
        ),
    )
    header = destination / "include" / "common.h"
    text = header.read_text(encoding="utf-8")
    expected_port = "#define SERVER_PORT 9090"
    if text.count(expected_port) != 1:
        raise RuntimeError("could not safely set the temporary engine port")
    header.write_text(
        text.replace(expected_port, f"#define SERVER_PORT {port}"),
        encoding="utf-8",
    )
    subprocess.run(
        ["make", "-C", str(destination), "bankserver"],
        check=True,
    )


def main():
    port = choose_port()
    with tempfile.TemporaryDirectory(prefix="bank-idle-timeout-") as temporary:
        engine = Path(temporary) / "engine"
        prepare_engine(engine, port)

        env = os.environ.copy()
        env["ENGINE_IDLE_TIMEOUT_SECONDS"] = str(IDLE_TIMEOUT_SECONDS)
        proc = subprocess.Popen(
            [str(engine / "bankserver")],
            cwd=engine,
            env=env,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.STDOUT,
        )
        idle_client = None
        replacement_client = None
        try:
            deadline = time.monotonic() + 10
            while time.monotonic() < deadline:
                if proc.poll() is not None:
                    raise RuntimeError(f"engine exited with status {proc.returncode}")
                try:
                    with socket.create_connection(("127.0.0.1", port), timeout=0.2):
                        break
                except OSError:
                    time.sleep(0.05)
            else:
                raise TimeoutError("temporary engine did not start")

            idle_client = socket.create_connection(("127.0.0.1", port), timeout=5)
            idle_client.settimeout(IDLE_TIMEOUT_SECONDS + 5)
            if not login(idle_client).startswith("OK|"):
                raise AssertionError("initial admin login failed")

            if idle_client.recv(1) != b"":
                raise AssertionError("idle engine connection was not closed")

            replacement_client = socket.create_connection(
                ("127.0.0.1", port), timeout=5
            )
            replacement_client.settimeout(5)
            if not login(replacement_client).startswith("OK|"):
                raise AssertionError("expired login guard was not released")

            print("PASS: idle connection closed and the same identity logged in again")
        finally:
            for client in (idle_client, replacement_client):
                if client:
                    client.close()
            proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait(timeout=5)


if __name__ == "__main__":
    main()
