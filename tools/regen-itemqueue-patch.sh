#!/usr/bin/env bash
# regen-itemqueue-patch.sh — re-cut patches/0008-core-item-update-queue-dedup.patch from the fork
# working tree at azerothcore-wotlk.
#
# CORE-ONLY patch, unlike every other patch in this repo: it touches
# src/server/game/Entities/Item/Item.cpp in the fork itself, NOT a file under
# modules/mod-playerbots. A plain `git -C "$AC" diff` with NO prefix rewriting is therefore
# correct — the a/src/... paths already resolve from the fork root, which is where setup.sh's
# apply_patches invokes `git -C "$AC_DIR" apply`. Do NOT copy the --src-prefix/--dst-prefix flags
# from the module regen scripts; they would corrupt the paths. The gate below enforces this.
#
# NOT baseline-aware: no other patch (0001-0007) touches Item.cpp, so a scoped diff of that one
# path emits exactly 0008's hunk. If a future patch starts touching Item.cpp, this script MUST
# become baseline-aware or it will silently embed the other patch's hunks (the trap that bit the
# 0004 regen).
#
# LITERAL PATHS ONLY on every git line — the shell does not word-split an unquoted $var, so a
# `for f in $FILES` loop would cut an empty patch (this trap has bitten the WG/publish recipes).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
AC="$ROOT/azerothcore-wotlk"
OUT="$ROOT/patches/0008-core-item-update-queue-dedup.patch"

[[ -d "$AC/.git" ]] || { echo "ERROR: $AC is not a git clone (run setup.sh first)" >&2; exit 1; }
[[ -f "$AC/src/server/game/Entities/Item/Item.cpp" ]] \
  || { echo "ERROR: missing core Item.cpp in the fork worktree" >&2; exit 1; }

# The edit must actually be present, or we would cut an empty patch over a good one and silently
# reintroduce the duplicate-queue-entry crash on the next fork reset.
grep -q "Re-adopt an existing slot" "$AC/src/server/game/Entities/Item/Item.cpp" \
  || { echo "ERROR: Item.cpp has no dedup scan — 0008 not applied to the worktree?" >&2; exit 1; }

git -C "$AC" diff -- src/server/game/Entities/Item/Item.cpp > "$OUT"

# Gates.
[[ -s "$OUT" ]] || { echo "GATE FAIL: generated patch is empty" >&2; exit 1; }
[[ "$(grep -c '^diff --git' "$OUT")" -eq 1 ]] || { echo "GATE FAIL: expected exactly 1 file in the patch" >&2; exit 1; }
grep -q "b/src/server/game/Entities/Item/Item.cpp" "$OUT" \
  || { echo "GATE FAIL: patch path is not fork-root relative — did you add --dst-prefix?" >&2; exit 1; }
for needle in "AddToUpdateQueueOf" "uQueuePos = int32(i)" "push_back"; do
  grep -q "$needle" "$OUT" || { echo "GATE FAIL: patch missing expected content: $needle" >&2; exit 1; }
done
# The fix must NOT touch RemoveFromUpdateQueueOf — an earlier, reverted attempt did, and changing
# that function's early-outs altered a load-bearing core invariant.
grep -q "^+.*RemoveFromUpdateQueueOf" "$OUT" \
  && { echo "GATE FAIL: patch modifies RemoveFromUpdateQueueOf — out of scope for 0008" >&2; exit 1; }

echo "OK: wrote $OUT ($(wc -l < "$OUT") lines)"
git -C "$AC" apply --stat "$OUT" | sed 's/^/    /'
