#!/usr/bin/env python3
"""Tiny Ollama-compatible bridge that forwards /api/generate to pi.

This is intentionally small and boring: AzerothCore's mod-playerbot-chatter already
speaks Ollama's /api/generate JSON shape, so this process pretends to be Ollama and
uses `pi -p` under the hood.

Run on the host as the user that is logged into pi/OpenAI/Codex, then point
PlayerbotChatter.Url / CHATTER_URL at http://HOST:11435/api/generate.
"""

from __future__ import annotations

import json
import os
import re
import shlex
import subprocess
import sys
import threading
import time
from collections import deque
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from typing import Any

HOST = os.environ.get("PI_BRIDGE_HOST", "127.0.0.1")
PORT = int(os.environ.get("PI_BRIDGE_PORT", "11435"))
PI_BIN = os.environ.get("PI_BRIDGE_PI_BIN", "pi")
PROVIDER = os.environ.get("PI_BRIDGE_PROVIDER", "")
MODEL = os.environ.get("PI_BRIDGE_MODEL", "")
THINKING = os.environ.get("PI_BRIDGE_THINKING", "off")
TIMEOUT = float(os.environ.get("PI_BRIDGE_TIMEOUT_SECONDS", "75"))
MAX_CONCURRENT = int(os.environ.get("PI_BRIDGE_MAX_CONCURRENT", "1"))
MAX_PER_MINUTE = int(os.environ.get("PI_BRIDGE_MAX_PER_MINUTE", "10"))
MAX_PER_HOUR = int(os.environ.get("PI_BRIDGE_MAX_PER_HOUR", "200"))
MAX_CHARS = int(os.environ.get("PI_BRIDGE_MAX_CHARS", "220"))
ONE_LINE = os.environ.get("PI_BRIDGE_ONE_LINE", "1") not in ("0", "false", "False", "no")
EXTRA_ARGS = shlex.split(os.environ.get("PI_BRIDGE_EXTRA_ARGS", ""))
DEBUG = os.environ.get("PI_BRIDGE_DEBUG", "0") in ("1", "true", "True", "yes")

# Extra guardrail appended to whatever system prompt AzerothCore sends.
BRIDGE_SYSTEM_TAIL = os.environ.get(
    "PI_BRIDGE_SYSTEM_TAIL",
    "\n\nYou are being used as an in-game WoW chat line generator. "
    "Return only the chat message text. Do not explain, preface, number, quote, "
    "or format the answer. Keep it short.",
)

semaphore = threading.BoundedSemaphore(MAX_CONCURRENT)
rate_lock = threading.Lock()
minute_hits: deque[float] = deque()
hour_hits: deque[float] = deque()


def log(msg: str) -> None:
    print(f"[pi-ollama-bridge] {msg}", file=sys.stderr, flush=True)


def allowed_by_rate_limit() -> tuple[bool, str]:
    now = time.time()
    with rate_lock:
        while minute_hits and now - minute_hits[0] > 60:
            minute_hits.popleft()
        while hour_hits and now - hour_hits[0] > 3600:
            hour_hits.popleft()

        if MAX_PER_MINUTE > 0 and len(minute_hits) >= MAX_PER_MINUTE:
            return False, "minute"
        if MAX_PER_HOUR > 0 and len(hour_hits) >= MAX_PER_HOUR:
            return False, "hour"

        minute_hits.append(now)
        hour_hits.append(now)
        return True, ""


def sanitize_response(text: str) -> str:
    text = text.replace("\r\n", "\n").replace("\r", "\n").strip()
    text = re.sub(r"(?is)<think>.*?</think>", "", text)
    text = re.sub(r"(?is)</?think>", "", text).strip()

    if ONE_LINE:
        # Pick the first useful line. This protects against pi/model explanations.
        lines = [line.strip() for line in text.split("\n") if line.strip()]
        text = lines[0] if lines else ""

    # Common prefixes/models sometimes add despite instructions.
    text = re.sub(r"^(assistant|bot|reply|response)\s*:\s*", "", text, flags=re.I).strip()
    text = text.strip('"“”')
    text = re.sub(r"[`*]+", "", text).strip()

    if MAX_CHARS > 0 and len(text) > MAX_CHARS:
        text = text[:MAX_CHARS]
        cut = text.rfind(" ")
        if cut > MAX_CHARS // 2:
            text = text[:cut]
        text = text.rstrip(" ,.;:-")
    return text


def build_pi_command(system_prompt: str, prompt: str) -> list[str]:
    cmd = [
        PI_BIN,
        "--print",
        "--no-tools",
        "--no-context-files",
        "--no-skills",
        "--no-extensions",
        "--no-prompt-templates",
        "--no-themes",
        "--no-session",
        "--thinking",
        THINKING,
        "--system-prompt",
        system_prompt + BRIDGE_SYSTEM_TAIL,
    ]
    if PROVIDER:
        cmd += ["--provider", PROVIDER]
    if MODEL:
        cmd += ["--model", MODEL]
    cmd += EXTRA_ARGS
    cmd.append(prompt)
    return cmd


def call_pi(system_prompt: str, prompt: str) -> str:
    ok, bucket = allowed_by_rate_limit()
    if not ok:
        log(f"rate limited by {bucket} limit; returning empty response")
        return ""

    if not semaphore.acquire(timeout=0.1):
        log("concurrency limit hit; returning empty response")
        return ""
    try:
        cmd = build_pi_command(system_prompt, prompt)
        if DEBUG:
            log("running: " + " ".join(shlex.quote(c) for c in cmd[:12]) + " ...")
        proc = subprocess.run(
            cmd,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=TIMEOUT,
            cwd=os.environ.get("PI_BRIDGE_CWD", os.path.expanduser("~")),
        )
        if proc.returncode != 0:
            log(f"pi exited {proc.returncode}: {proc.stderr.strip()[-1000:]}")
            return ""
        if DEBUG and proc.stderr.strip():
            log("pi stderr: " + proc.stderr.strip()[-1000:])
        return sanitize_response(proc.stdout)
    except subprocess.TimeoutExpired:
        log(f"pi timed out after {TIMEOUT}s")
        return ""
    except Exception as exc:  # noqa: BLE001 - bridge should fail closed, not crash world chat
        log(f"pi call failed: {exc}")
        return ""
    finally:
        semaphore.release()


class Handler(BaseHTTPRequestHandler):
    server_version = "pi-ollama-bridge/0.1"

    def log_message(self, fmt: str, *args: Any) -> None:
        if DEBUG:
            log(f"{self.address_string()} - {fmt % args}")

    def send_json(self, status: int, payload: dict[str, Any]) -> None:
        body = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def read_json(self) -> dict[str, Any]:
        length = int(self.headers.get("Content-Length", "0"))
        if length <= 0:
            return {}
        raw = self.rfile.read(length)
        return json.loads(raw.decode("utf-8"))

    def do_GET(self) -> None:  # noqa: N802 - stdlib API name
        if self.path in ("/", "/api/version"):
            self.send_json(200, {"version": "pi-ollama-bridge"})
        elif self.path == "/api/tags":
            name = MODEL or "pi-default"
            self.send_json(200, {"models": [{"name": name, "model": name, "modified_at": "", "size": 0}]})
        else:
            self.send_json(404, {"error": "not found"})

    def do_POST(self) -> None:  # noqa: N802 - stdlib API name
        if self.path == "/api/generate":
            self.handle_generate()
        elif self.path == "/api/chat":
            self.handle_chat()
        else:
            self.send_json(404, {"error": "not found"})

    def handle_generate(self) -> None:
        try:
            req = self.read_json()
        except Exception as exc:  # noqa: BLE001
            self.send_json(400, {"error": f"bad json: {exc}"})
            return

        system_prompt = str(req.get("system") or "")
        prompt = str(req.get("prompt") or "")
        if not prompt.strip():
            self.send_json(200, {"response": "", "done": True})
            return

        started = time.time()
        response = call_pi(system_prompt, prompt)
        elapsed_ns = int((time.time() - started) * 1_000_000_000)
        self.send_json(
            200,
            {
                "model": req.get("model") or MODEL or "pi-default",
                "created_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
                "response": response,
                "done": True,
                "total_duration": elapsed_ns,
            },
        )

    def handle_chat(self) -> None:
        """Minimal Ollama /api/chat compatibility for manual testing."""
        try:
            req = self.read_json()
        except Exception as exc:  # noqa: BLE001
            self.send_json(400, {"error": f"bad json: {exc}"})
            return

        messages = req.get("messages") or []
        system_parts: list[str] = []
        prompt_parts: list[str] = []
        for msg in messages:
            role = str(msg.get("role") or "user")
            content = str(msg.get("content") or "")
            if role == "system":
                system_parts.append(content)
            else:
                prompt_parts.append(f"{role}: {content}")
        response = call_pi("\n".join(system_parts), "\n".join(prompt_parts))
        self.send_json(
            200,
            {
                "model": req.get("model") or MODEL or "pi-default",
                "created_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
                "message": {"role": "assistant", "content": response},
                "done": True,
            },
        )


def main() -> int:
    log(
        f"listening on {HOST}:{PORT}; pi={PI_BIN!r}; provider={PROVIDER or '(default)'}; "
        f"model={MODEL or '(default)'}; max_concurrent={MAX_CONCURRENT}; "
        f"rate={MAX_PER_MINUTE}/min {MAX_PER_HOUR}/hour"
    )
    httpd = ThreadingHTTPServer((HOST, PORT), Handler)
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        log("stopping")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
