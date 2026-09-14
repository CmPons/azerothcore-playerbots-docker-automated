#!/usr/bin/env python3
"""Offline production-header syntax check; no CMake server build or service operations."""
import argparse
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
CORE = ROOT / "azerothcore-wotlk"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("sources", nargs="+")
    args = parser.parse_args()
    paths = set()
    for source in ("src/common", "src/server/game", "src/server/shared", "src/server/database",
                   "modules/mod-playerbots/src"):
        paths.update(p.parent for p in (CORE / source).rglob("*.h"))
    for source in ("deps/fmt/include", "deps/g3dlite/include", "deps/recastnavigation/Detour/Include",
                   "deps/recastnavigation/Recast/Include", "deps/SFMT", "deps/utf8cpp", "deps/fkYAML/include"):
        paths.add(CORE / source)
    for name in ("boost", "openssl"):
        found = sorted(Path("/nix/store").glob(f"*-{name}-*-dev"))
        if found:
            paths.add(found[-1] / "include")
    command = ["g++", "-std=gnu++20", "-DMOD_PLAYERBOTS", "-fsyntax-only",
               "-DACORE_API_USE_DYNAMIC_LINKING=0", "-DBOOST_ASIO_NO_DEPRECATED",
               "-DFMT_USE_NONTYPE_TEMPLATE_ARGS=0"]
    command += ["-I" + str(p) for p in sorted(paths)]
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "syntax-arguments.json").write_text(json.dumps(command, indent=2) + "\n")
    failed = False
    for source in args.sources:
        result = subprocess.run(command + [str(CORE / source)], text=True, capture_output=True)
        (args.output / (Path(source).name + ".log")).write_text(result.stdout + result.stderr)
        print(source, result.returncode, flush=True)
        if result.returncode:
            print(result.stderr[:10000])
            failed = True
    raise SystemExit(int(failed))


if __name__ == "__main__":
    main()
