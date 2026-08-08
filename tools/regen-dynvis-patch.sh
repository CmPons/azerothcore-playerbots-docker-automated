#!/usr/bin/env bash
# regen-dynvis-patch.sh — re-cut patches/0010-core-dynamic-visibility-extend.patch from the
# fork working tree at azerothcore-wotlk.
#
# CORE-ONLY patch (like 0008/0009, unlike every module patch): it touches
# src/server/game/Misc/DynamicVisibility.h in the fork itself. A plain
# `git -C "$AC" diff` with NO prefix rewriting is therefore correct — the a/src/... paths already
# resolve from the fork root, which is where setup.sh's apply_patches invokes
# `git -C "$AC_DIR" apply`. Do NOT copy the --src-prefix/--dst-prefix flags from the module regen
# scripts; they would corrupt the paths. The gate below enforces this.
#
# NOT baseline-aware: no other patch touches DynamicVisibility.h, so a scoped diff of that one
# path emits exactly 0010's hunk. If a future patch starts touching it, this script MUST become
# baseline-aware or it will silently embed the other patch's hunks (the trap that bit the 0004
# regen).
#
# WHAT 0010 IS: pussywizard's DynamicVisibilityMgr table stock tops out at "3000+" sessions, so a
# 4000-5000-bot population runs permanently at that bucket with no further throttling headroom.
# 0010 extends the table to 11 intervals (3500/4000/4500/5000+), scaling continent/instance/raid
# notify delays up while keeping bg mild and arena untouched.
#
# LITERAL PATHS ONLY on every git line — the shell does not word-split an unquoted $var, so a
# `for f in $FILES` loop would cut an empty patch (this trap has bitten the WG/publish recipes).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
AC="$ROOT/azerothcore-wotlk"
OUT="$ROOT/patches/0010-core-dynamic-visibility-extend.patch"

[[ -d "$AC/.git" ]] || { echo "ERROR: $AC is not a git clone (run setup.sh first)" >&2; exit 1; }
[[ -f "$AC/src/server/game/Misc/DynamicVisibility.h" ]] \
  || { echo "ERROR: missing core DynamicVisibility.h in the fork worktree" >&2; exit 1; }

# The edit must actually be present, or we would cut an empty patch over a good one and silently
# drop the extension on the next fork reset.
grep -q "VISIBILITY_SETTINGS_MAX_INTERVAL_NUM 11" \
  "$AC/src/server/game/Misc/DynamicVisibility.h" \
  || { echo "ERROR: DynamicVisibility.h still has the 7-interval table — 0010 not applied?" >&2; exit 1; }

git -C "$AC" diff -- src/server/game/Misc/DynamicVisibility.h > "$OUT"

# Gates.
[[ -s "$OUT" ]] || { echo "GATE FAIL: generated patch is empty" >&2; exit 1; }
[[ "$(grep -c '^diff --git' "$OUT")" -eq 1 ]] || { echo "GATE FAIL: expected exactly 1 file in the patch" >&2; exit 1; }
grep -q "b/src/server/game/Misc/DynamicVisibility.h" "$OUT" \
  || { echo "GATE FAIL: patch path is not fork-root relative — did you add --dst-prefix?" >&2; exit 1; }
for needle in "VISIBILITY_SETTINGS_MAX_INTERVAL_NUM 11" "5000+" "3500-3999"; do
  grep -q -- "$needle" "$OUT" || { echo "GATE FAIL: patch missing expected content: $needle" >&2; exit 1; }
done
# The interval WIDTH must stay 500 — DynamicVisibilityMgr::Update derives bucket boundaries from
# it, and the new rows are meaningless if someone also changes the stride.
if grep -E '^\+' "$OUT" | grep -q "VISIBILITY_SETTINGS_PLAYER_INTERVAL"; then
  echo "GATE FAIL: patch changes VISIBILITY_SETTINGS_PLAYER_INTERVAL — out of scope for 0010" >&2; exit 1
fi

echo "OK: wrote $OUT ($(wc -l < "$OUT") lines)"
git -C "$AC" apply --stat "$OUT" | sed 's/^/    /'
