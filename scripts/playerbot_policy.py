#!/usr/bin/env python3
"""Trusted-host Cthun policy publisher. Files only: no server, SQL or gameplay commands."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import secrets
import stat
import subprocess
import time

MAX_SOURCE = 32768


def atomic(path, data):
    temporary = path.with_name(path.name + "." + secrets.token_hex(8) + ".tmp")
    try:
        with temporary.open("xb") as output:
            output.write(data)
            output.flush()
            os.fsync(output.fileno())
        os.replace(temporary, path)
    finally:
        temporary.unlink(missing_ok=True)


def revision(source):
    return hashlib.sha256(source).hexdigest()


def source_bytes(path):
    descriptor = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    with os.fdopen(descriptor, "rb") as source_file:
        metadata = os.fstat(source_file.fileno())
        if not stat.S_ISREG(metadata.st_mode) or metadata.st_size > MAX_SOURCE:
            raise ValueError("source must be a bounded regular file")
        source = source_file.read(MAX_SOURCE + 1)
    if not source or len(source) > MAX_SOURCE:
        raise ValueError("source byte budget")
    return source


def checked(path, checker):
    source = source_bytes(path)
    # Check the SAME immutable bytes that will be published, not a path that can change afterward.
    import tempfile
    with tempfile.NamedTemporaryFile(suffix=".lua") as snapshot:
        snapshot.write(source)
        snapshot.flush()
        subprocess.run([str(checker.resolve()), snapshot.name], check=True, timeout=10)
    return source


def read_status(directory, scope):
    if not re.fullmatch(r"531-\d+-\d+", scope):
        raise ValueError("scope must come from a current status filename")
    path = directory / (scope + ".json")
    if not path.exists():
        return {"scope": scope, "state": "unloaded/not observed"}
    if path.is_symlink() or path.stat().st_size > 4096:
        raise ValueError("invalid status file")
    data = json.loads(path.read_text())
    age = time.time() - data["updated"]
    data["age_seconds"] = round(age, 1)
    data["state"] = ("unloaded" if data["destroyed"] else "stale/unloaded" if age > 5
                     else "queued between pulls" if data["queued"] else "active")
    return data


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=["check", "publish", "status", "revert"])
    parser.add_argument("source", nargs="?", type=Path)
    parser.add_argument("--directory", type=Path)
    parser.add_argument("--status-directory", type=Path)
    parser.add_argument("--scope")
    parser.add_argument("--expect-active")
    parser.add_argument("--revision")
    parser.add_argument("--checker", type=Path)
    args = parser.parse_args(argv)
    if args.command == "status":
        if not args.status_directory or not args.scope:
            parser.error("status requires --status-directory and --scope (read filenames from that directory)")
        print(json.dumps(read_status(args.status_directory, args.scope), indent=2))
        return
    if not args.checker:
        parser.error("--checker must name the offline runner linked to the pinned production Lua runtime")
    if args.command == "revert":
        if not args.directory or not args.revision or not re.fullmatch(r"[0-9a-f]{64}", args.revision):
            parser.error("revert requires --directory and --revision")
        args.source = args.directory / "revisions" / (args.revision + ".lua")
    if not args.source:
        parser.error("source required")
    source = checked(args.source, args.checker)
    digest = revision(source)
    if args.command == "check":
        print(digest)
        return
    if args.command == "revert" and args.revision != digest:
        raise ValueError("retained revision hash mismatch")
    if not args.directory or not args.status_directory or not args.scope or not args.expect_active:
        parser.error("publish/revert require directory, status-directory, scope and expect-active")
    if not re.fullmatch(r"native|[0-9a-f]{64}", args.expect_active):
        raise ValueError("invalid expected revision")
    status = read_status(args.status_directory, args.scope)
    if status["state"] in ("unloaded", "stale/unloaded", "unloaded/not observed"):
        raise ValueError("scope not freshly loaded; do not publish to an old generation")
    if status["active"] != args.expect_active:
        raise ValueError("expected revision mismatch")
    # Parent directory mount, not a single-file mount: rename must be visible inside worldserver.
    for folder in ("revisions", "requests"):
        path = args.directory / folder
        path.mkdir(parents=True, exist_ok=True)
        if path.is_symlink() or args.directory.is_symlink():
            raise ValueError("symlink mailbox directory")
    bundle = args.directory / "revisions" / (digest + ".lua")
    if bundle.exists():
        if bundle.is_symlink() or source_bytes(bundle) != source:
            raise ValueError("immutable revision collision")
    else:
        # Atomic, exclusive link of fully written immutable bytes; never overwrite a revision.
        temporary = bundle.with_name(bundle.name + "." + secrets.token_hex(8) + ".tmp")
        try:
            with temporary.open("xb") as output:
                output.write(source)
                output.flush()
                os.fsync(output.fileno())
            temporary.chmod(0o444)
            os.link(temporary, bundle)
        finally:
            temporary.unlink(missing_ok=True)
    request = f"{secrets.token_hex(8)} {args.expect_active} {digest}\n".encode()
    atomic(args.directory / "requests" / (args.scope + ".txt"), request)
    print(json.dumps({"scope": args.scope, "requested": digest,
                      "state": "published; not yet queued/active until instance acknowledgement"}))


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, subprocess.SubprocessError) as error:
        raise SystemExit(str(error))
