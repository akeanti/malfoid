#!/usr/bin/env python3
"""Serve docs/ locally with caching disabled, so edited ES modules always reload.

    python tools/serve_docs.py [port]     # default 8642, http://localhost:8642

Web Serial and ESP Web Tools work on localhost because browsers treat it as a
secure context.
"""
import functools
import http.server
import sys
from pathlib import Path

DOCS = Path(__file__).resolve().parent.parent / "docs"


class NoCacheHandler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Cache-Control", "no-store")
        super().end_headers()


def main() -> None:
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8642
    handler = functools.partial(NoCacheHandler, directory=str(DOCS))
    with http.server.ThreadingHTTPServer(("127.0.0.1", port), handler) as server:
        print(f"Serving {DOCS} at http://localhost:{port}")
        server.serve_forever()


if __name__ == "__main__":
    main()
