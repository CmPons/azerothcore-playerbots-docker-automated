#!/usr/bin/env python3
"""Trusted-host raid policy publisher. Files only: no server, SQL or gameplay commands."""
import argparse
from contextlib import contextmanager
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import secrets
import stat
import subprocess
import sys
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
        sync_directory(path.parent)
    finally:
        temporary.unlink(missing_ok=True)


def sync_directory(path):
    descriptor = os.open(path, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    try:
        os.fsync(descriptor)
    finally:
        os.close(descriptor)


@contextmanager
def publication_directory(directory):
    # Private by default; existing ACLs/modes are never replaced. Default ACLs, when installed,
    # propagate mapped-worldserver read access to newly created folders/files.
    previous = os.umask(0o077)
    try:
        for path in (directory, *(directory / name for name in ("revisions", "requests", "defaults"))):
            if path.is_symlink():
                raise ValueError("symlink mailbox directory")
            path.mkdir(parents=True, exist_ok=True)
        descriptor = os.open(directory / ".publish.lock",
                             os.O_RDWR | os.O_CREAT | os.O_NOFOLLOW | os.O_NONBLOCK, 0o600)
        with os.fdopen(descriptor, "wb") as lock:
            if not stat.S_ISREG(os.fstat(lock.fileno()).st_mode):
                raise ValueError("nonregular publication lock")
            fcntl.flock(lock, fcntl.LOCK_EX)
            yield
    finally:
        os.umask(previous)


def installed_default(directory):
    path = directory / "defaults" / "raid.txt"
    if not path.exists():
        return "none"
    data = source_bytes(path, 256)
    fields = data.decode("ascii").split()
    if (len(fields) != 4 or fields[:2] != ["1", "1"] or
            not re.fullmatch(r"[0-9a-f]{16}", fields[2]) or not re.fullmatch(r"[0-9a-f]{64}", fields[3])):
        raise ValueError("invalid installed default")
    return fields[2] + ":" + fields[3]


def retain(directory, digest, source):
    bundle = directory / "revisions" / (digest + ".lua")
    if bundle.exists():
        if bundle.is_symlink() or source_bytes(bundle) != source:
            raise ValueError("immutable revision collision")
        return
    temporary = bundle.with_name(bundle.name + "." + secrets.token_hex(8) + ".tmp")
    try:
        with temporary.open("xb") as output:
            output.write(source)
            output.flush()
            os.fsync(output.fileno())
        temporary.chmod(0o444)
        os.link(temporary, bundle)
        sync_directory(bundle.parent)
    finally:
        temporary.unlink(missing_ok=True)


def revision(source):
    return hashlib.sha256(source).hexdigest()


def source_bytes(path, maximum=MAX_SOURCE):
    descriptor = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    with os.fdopen(descriptor, "rb") as source_file:
        metadata = os.fstat(source_file.fileno())
        if not stat.S_ISREG(metadata.st_mode) or metadata.st_size > maximum:
            raise ValueError("source must be a bounded regular file")
        source = source_file.read(maximum + 1)
    if not source or len(source) > maximum:
        raise ValueError("source byte budget")
    return source


def checked(path, checker):
    source = source_bytes(path)
    print(f"Checking {path}", file=sys.stderr)
    # Check the SAME immutable bytes that will be published, not a path that can change afterward.
    import tempfile
    with tempfile.NamedTemporaryFile(suffix=".lua") as snapshot:
        snapshot.write(source)
        snapshot.flush()
        subprocess.run([str(checker.resolve()), snapshot.name], check=True, timeout=10, stdout=sys.stderr)
    return source


def read_status(directory, scope):
    if not re.fullmatch(r"\d+-\d+-\d+", scope):
        raise ValueError("scope must come from a current status filename")
    path = directory / (scope + ".json")
    if not path.exists():
        return {"scope": scope, "state": "unloaded/not observed"}
    if path.is_symlink() or path.stat().st_size > 16384:
        raise ValueError("invalid status file")
    data = json.loads(path.read_text())
    age = time.time() - data["updated"]
    data["age_seconds"] = round(age, 1)
    data["state"] = ("unloaded" if data["destroyed"] else "stale/unloaded" if age > 5
                     else "queued between pulls" if data["queued"]
                     else "native fallback" if data["active"] == "native" else "active")
    return data


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=["check", "publish-default", "publish", "status", "revert"])
    parser.add_argument("source", nargs="?", type=Path)
    parser.add_argument("--directory", type=Path)
    parser.add_argument("--status-directory", type=Path)
    parser.add_argument("--scope")
    parser.add_argument("--expect-active")
    parser.add_argument("--expect-default", help="optional installed publication identity CAS, or none")
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
    if not args.directory:
        parser.error("publication requires --directory")
    if args.command != "publish-default":
        if not args.status_directory or not args.scope or not args.expect_active:
            parser.error("publish/revert require status-directory, scope and expect-active")
        if not re.fullmatch(r"native|[0-9a-f]{64}", args.expect_active):
            raise ValueError("invalid expected revision")
    with publication_directory(args.directory):
        if args.command == "publish-default":
            # Last checked publisher wins unless the caller explicitly requests manifest CAS.
            # Without CAS a corrected valid publication can replace a malformed manifest.
            if args.expect_default is not None and installed_default(args.directory) != args.expect_default:
                raise ValueError("expected default publication mismatch")
            retain(args.directory, digest, source)
            nonce = secrets.token_hex(8)
            atomic(args.directory / "defaults" / "raid.txt", f"1 1 {nonce} {digest}\n".encode())
            print(json.dumps({"publication": nonce + ":" + digest, "requested": digest,
                              "state": "installed default; each relevant scope adopts once safe"}))
            return
        status = read_status(args.status_directory, args.scope)
        if status["state"] in ("unloaded", "stale/unloaded", "unloaded/not observed"):
            raise ValueError("scope not freshly loaded; do not publish to an old generation")
        if status["active"] != args.expect_active:
            raise ValueError("expected revision mismatch")
        publication = status.get("default_publication", "") or "none"
        if installed_default(args.directory) != publication:
            raise ValueError("scope has not observed current default publication")
        retain(args.directory, digest, source)
        request = f"{secrets.token_hex(8)} {args.expect_active} {digest} {publication}\n".encode()
        atomic(args.directory / "requests" / (args.scope + ".txt"), request)
        print(json.dumps({"scope": args.scope, "requested": digest, "default_publication": publication,
                          "state": "diagnostic published; expires on next default publication"}))



if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, subprocess.SubprocessError) as error:
        raise SystemExit(str(error))
