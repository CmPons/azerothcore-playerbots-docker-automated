#!/usr/bin/env bash
# regen-gameevent-patch.sh — re-cut patches/0013-core-gameevent-sai-sweep-skip.patch from the
# fork working tree at azerothcore-wotlk.
#
# CORE-ONLY patch (like 0008/0009/0010/0012): it touches
#   src/server/game/AI/SmartScripts/SmartScriptMgr.{h,cpp}
#   src/server/game/Events/GameEventMgr.cpp
# in the fork itself. A plain `git -C "$AC" diff` with NO prefix rewriting is therefore correct —
# the a/src/... paths already resolve from the fork root, where setup.sh's apply_patches invokes
# `git -C "$AC_DIR" apply`. Do NOT copy the --src-prefix/--dst-prefix flags from the module regen
# scripts; they would corrupt the paths. The gate below enforces this.
#
# NOT baseline-aware: no other patch touches these three files (verified against patches/0001-0012
# diff headers). If a future patch starts touching them, this script MUST become baseline-aware.
#
# WHAT 0013 IS: every game_event start/stop calls GameEventMgr::RunSmartAIScripts, which sweeps
# EVERY creature/GO in EVERY map's object store on the world thread to deliver
# SMART_EVENT_GAME_EVENT_START/END. With PreloadAllNonInstancedMapGrids=1 that sweep covers the
# whole world (~300ms world-tick stall, measured at 3200 bots), and short-cycle flavor events
# (Dalaran Minigob, hourly concert/bells/AT triggers) transition every ~5 minutes. Most of those
# transitions have NO SmartAI listener at all. 0013 has SmartAIMgr record which
# (game event id, start|end) pairs actually have listeners while loading smart_scripts, and
# RunSmartAIScripts returns immediately for unlistened transitions. Scripts that do listen are
# unaffected.
#
# LITERAL PATHS ONLY on every git line — the shell does not word-split an unquoted $var, so a
# `for f in $FILES` loop would cut an empty patch (this trap has bitten the WG/publish recipes).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
AC="$ROOT/azerothcore-wotlk"
OUT="$ROOT/patches/0013-core-gameevent-sai-sweep-skip.patch"

[[ -d "$AC/.git" ]] || { echo "ERROR: $AC is not a git clone (run setup.sh first)" >&2; exit 1; }

# The edit must actually be present in all three files, or we would cut a partial/empty patch
# over a good one and silently drop the skip on the next fork reset.
grep -q "HasGameEventListener" "$AC/src/server/game/AI/SmartScripts/SmartScriptMgr.h" \
  || { echo "ERROR: SmartScriptMgr.h has no HasGameEventListener — 0013 not applied?" >&2; exit 1; }
grep -q "mGameEventStartListeners" "$AC/src/server/game/AI/SmartScripts/SmartScriptMgr.cpp" \
  || { echo "ERROR: SmartScriptMgr.cpp does not populate the listener sets — 0013 not applied?" >&2; exit 1; }
grep -q "HasGameEventListener" "$AC/src/server/game/Events/GameEventMgr.cpp" \
  || { echo "ERROR: GameEventMgr.cpp has no listener gate — 0013 not applied?" >&2; exit 1; }

git -C "$AC" diff -- \
  src/server/game/AI/SmartScripts/SmartScriptMgr.h \
  src/server/game/AI/SmartScripts/SmartScriptMgr.cpp \
  src/server/game/Events/GameEventMgr.cpp > "$OUT"

# Gates.
[[ -s "$OUT" ]] || { echo "GATE FAIL: generated patch is empty" >&2; exit 1; }
[[ "$(grep -c '^diff --git' "$OUT")" -eq 3 ]] || { echo "GATE FAIL: expected exactly 3 files in the patch" >&2; exit 1; }
grep -q "b/src/server/game/Events/GameEventMgr.cpp" "$OUT" \
  || { echo "GATE FAIL: patch path is not fork-root relative — did you add --dst-prefix?" >&2; exit 1; }
for needle in "HasGameEventListener" "mGameEventStartListeners" "mGameEventEndListeners" "SMART_EVENT_GAME_EVENT_START"; do
  grep -q -- "$needle" "$OUT" || { echo "GATE FAIL: patch missing expected content: $needle" >&2; exit 1; }
done
# The sweep itself must stay intact — 0013 only ADDS an early-out. If DoForAllMaps shows up as a
# removed line, the patch is deleting behavior instead of gating it.
if grep -qE '^-.*DoForAllMaps' "$OUT"; then
  echo "GATE FAIL: patch removes the DoForAllMaps sweep — out of scope for 0013" >&2; exit 1
fi

echo "OK: wrote $OUT ($(wc -l < "$OUT") lines)"
git -C "$AC" apply --stat "$OUT" | sed 's/^/    /'
