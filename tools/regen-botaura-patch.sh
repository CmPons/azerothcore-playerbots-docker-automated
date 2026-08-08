#!/usr/bin/env bash
# regen-botaura-patch.sh — re-cut patches/0012-core-bot-aura-batching.patch from the fork
# working tree at azerothcore-wotlk.
#
# CORE-ONLY patch (like 0008/0009/0010, unlike every module patch): it touches
# src/server/game/Entities/Unit/Unit.{h,cpp} in the fork itself. A plain `git -C "$AC" diff`
# with NO prefix rewriting is therefore correct — the a/src/... paths already resolve from the
# fork root, which is where setup.sh's apply_patches invokes `git -C "$AC_DIR" apply`. Do NOT
# copy the --src-prefix/--dst-prefix flags from the module regen scripts; they would corrupt
# the paths. The gate below enforces this.
#
# NOT baseline-aware: no other patch touches Unit.h/Unit.cpp (verified against patches/0001-0011
# diff headers). If a future patch starts touching them, this script MUST become baseline-aware
# or it will silently embed the other patch's hunks (the trap that bit the 0004 regen).
#
# WHAT 0012 IS: at 3200 bots a host perf profile showed ~20% of all worldserver CPU in the
# per-tick aura sweeps (Aura::UpdateOwner + AuraEffect::Update + Unit::_UpdateSpells + the
# rb-tree iteration of the aura multimaps) — thousands of fully-buffed bots walking every owned
# aura every map tick to find timers that aren't due. All aura timing is diff-driven, so 0012
# accumulates the diff for BOT sessions (WorldSession::IsBot()) and runs the sweeps at ~100ms
# cadence with the summed diff: identical timing semantics, up to ~90ms added latency on
# periodic ticks/expiry, real players untouched. The current-spell pointer bookkeeping at the
# top of _UpdateSpells stays per-tick — it is order-sensitive with the event system (see the
# WARNING in Unit::Update).
#
# LITERAL PATHS ONLY on every git line — the shell does not word-split an unquoted $var, so a
# `for f in $FILES` loop would cut an empty patch (this trap has bitten the WG/publish recipes).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
AC="$ROOT/azerothcore-wotlk"
OUT="$ROOT/patches/0012-core-bot-aura-batching.patch"

[[ -d "$AC/.git" ]] || { echo "ERROR: $AC is not a git clone (run setup.sh first)" >&2; exit 1; }

# The edit must actually be present in BOTH files, or we would cut a partial/empty patch over a
# good one and silently drop the batching on the next fork reset.
grep -q "m_botAuraUpdateTimer" "$AC/src/server/game/Entities/Unit/Unit.h" \
  || { echo "ERROR: Unit.h has no m_botAuraUpdateTimer member — 0012 not applied?" >&2; exit 1; }
grep -q "BOT_AURA_UPDATE_INTERVAL" "$AC/src/server/game/Entities/Unit/Unit.cpp" \
  || { echo "ERROR: Unit.cpp has no batching gate — 0012 not applied?" >&2; exit 1; }

git -C "$AC" diff -- src/server/game/Entities/Unit/Unit.h src/server/game/Entities/Unit/Unit.cpp > "$OUT"

# Gates.
[[ -s "$OUT" ]] || { echo "GATE FAIL: generated patch is empty" >&2; exit 1; }
[[ "$(grep -c '^diff --git' "$OUT")" -eq 2 ]] || { echo "GATE FAIL: expected exactly 2 files in the patch" >&2; exit 1; }
grep -q "b/src/server/game/Entities/Unit/Unit.cpp" "$OUT" \
  || { echo "GATE FAIL: patch path is not fork-root relative — did you add --dst-prefix?" >&2; exit 1; }
for needle in "m_botAuraUpdateTimer" "BOT_AURA_UPDATE_INTERVAL" "IsBot()"; do
  grep -q -- "$needle" "$OUT" || { echo "GATE FAIL: patch missing expected content: $needle" >&2; exit 1; }
done
# Scope: 0012 must not touch the order-sensitive current-spell bookkeeping or auto-repeat path.
# Only inspect added CODE lines (comments may legitimately mention them).
if grep -E '^\+[[:space:]]*[^/[:space:]+]' "$OUT" | grep -qE "m_currentSpells|_UpdateAutoRepeatSpell"; then
  echo "GATE FAIL: patch modifies current-spell handling — out of scope for 0012" >&2; exit 1
fi
# The gate must be inside _UpdateSpells (hunk header shows the enclosing function).
grep -q "^@@.*_UpdateSpells" "$OUT" \
  || { echo "GATE FAIL: Unit.cpp hunk is not inside Unit::_UpdateSpells" >&2; exit 1; }

echo "OK: wrote $OUT ($(wc -l < "$OUT") lines)"
git -C "$AC" apply --stat "$OUT" | sed 's/^/    /'
