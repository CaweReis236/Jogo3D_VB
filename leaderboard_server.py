#!/usr/bin/env python3
"""Vector Search — ranking global experimental (Python stdlib + SQLite).

Uso local: python3 leaderboard_server.py --host 127.0.0.1 --port 8765
Para multiplos jogadores, hospede em servidor acessivel publicamente com HTTPS.
O endpoint HTTP simples nao possui autenticacao/anticheat: NAO e ranking competitivo seguro.
"""
from __future__ import annotations

import argparse
import re
import sqlite3
from datetime import datetime, timezone
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlsplit

NAME = re.compile(r"[a-zA-Z0-9_-]{1,20}\Z")
MAX_BODY_BYTES = 512
DB_FILE = Path(__file__).with_name("vector_search_global.sqlite3")


def open_db() -> sqlite3.Connection:
    connection = sqlite3.connect(DB_FILE, timeout=6)
    connection.execute("""
        CREATE TABLE IF NOT EXISTS scores (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            time_ms INTEGER NOT NULL,
            seed INTEGER NOT NULL,
            created_utc TEXT NOT NULL
        )
    """)
    connection.commit()
    return connection


class Handler(BaseHTTPRequestHandler):
    server_version = "VectorSearchLeaderboard/1.0"

    def send_text(self, code: int, body: str) -> None:
        encoded = body.encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "text/plain; charset=utf-8")
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("Content-Length", str(len(encoded)))
        self.end_headers()
        self.wfile.write(encoded)

    def do_GET(self) -> None:  # noqa: N802 (http.server convention)
        path = urlsplit(self.path).path
        if path == "/health":
            self.send_text(200, "VECTOR SEARCH SERVER OK\n")
            return
        if path != "/ranking":
            self.send_text(404, "not found\n")
            return
        with open_db() as db:
            rows = db.execute(
                "SELECT name, time_ms, seed FROM scores "
                "ORDER BY time_ms ASC, id ASC LIMIT 100"
            ).fetchall()
        content = "VS-RANKING-1\n" + "".join(
            f"{name}\t{ms}\t{seed}\n" for name, ms, seed in rows
        )
        self.send_text(200, content)

    def do_POST(self) -> None:  # noqa: N802
        if urlsplit(self.path).path != "/submit":
            self.send_text(404, "not found\n")
            return
        try:
            length = int(self.headers.get("Content-Length", "0"))
            if not 0 < length <= MAX_BODY_BYTES:
                raise ValueError("invalid body size")
            params = parse_qs(self.rfile.read(length).decode("ascii"), strict_parsing=True)
            name = params["name"][0]
            milliseconds = int(params["time_ms"][0])
            seed = int(params["seed"][0])
            if not NAME.fullmatch(name) or not 1 <= milliseconds <= 86_400_000_000 or not 0 <= seed <= 0xFFFFFFFF:
                raise ValueError("invalid score")
        except (ValueError, KeyError, UnicodeDecodeError, IndexError):
            self.send_text(400, "invalid parameters\n")
            return
        with open_db() as db:
            db.execute(
                "INSERT INTO scores(name, time_ms, seed, created_utc) VALUES (?, ?, ?, ?)",
                (name, milliseconds, seed, datetime.now(timezone.utc).isoformat()),
            )
        self.send_text(200, "recorded\n")


def main() -> None:
    parser = argparse.ArgumentParser(description="Servidor de ranking Vector Search")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8765)
    arguments = parser.parse_args()
    with open_db():
        pass
    print(f"Ranking Vector Search em http://{arguments.host}:{arguments.port} — SQLite: {DB_FILE}")
    print("ATENCAO: sem TLS, autenticacao ou anticheat. Use um proxy HTTPS no servidor publico.")
    server = ThreadingHTTPServer((arguments.host, arguments.port), Handler)
    try:
        server.serve_forever(poll_interval=0.3)
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
