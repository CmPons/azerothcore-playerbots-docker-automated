#!/usr/bin/env bash
# regen-wg-siege-patch.sh — re-cut patches/0004-playerbot-wintergrasp-siege.patch from the fork
# working tree. 0004 spans TWELVE mod-playerbots files PLUS the core BattlefieldWG.cpp hunk.
#
# BASELINE-AWARE (see regen-arena-patch.sh for the technique). The fork applies 0003/0004/0005
# UNSTAGED, so a naive per-file `git diff` emits every applied patch's hunks:
#   - AiFactory.cpp / PlayerbotAI.cpp / RandomPlayerbotMgr.cpp are ALSO in 0003 -> baseline them
#     at pristine+0003 so only 0004's hunks are emitted.
#   - ActionContext.h is ALSO in 0005 (applied AFTER 0004) -> rebuild a pristine+0004-ONLY worktree
#     copy for the diff (from the current 0004 patch's own ActionContext.h hunk).
#   - The core BattlefieldWG.cpp hunk lives in the FORK-ROOT repo and MUST be appended, or a
#     fork-only regen silently drops it (this bit production once).
# LITERAL PATHS ONLY on every git/cp line.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
AC="$ROOT/azerothcore-wotlk"
PB="$AC/modules/mod-playerbots"
OUT="$ROOT/patches/0004-playerbot-wintergrasp-siege.patch"
P0003="$ROOT/patches/0003-playerbot-wintergrasp.patch"
# PlayerbotAI.cpp is a shared registration file also touched by LATER patches (0011/0014/0016).
# 0004 is a MIDDLE patch, so those can't be added to the pristine+0003 baseline (their hunks were
# cut WITH 0004 applied, and won't apply to a tree that lacks it). Instead we REVERSE-apply their
# PlayerbotAI.cpp hunks from the diff's worktree copy, leaving pristine+0003+0004 — see step 5b.
# Keep this list current: every patch after 0004 that edits PlayerbotAI.cpp MUST be listed, or the
# regen silently re-absorbs its hunks into 0004 (the contamination trap that shipped a broken 0004).
P_LATER_PBAI=("$ROOT/patches/0016-playerbot-aq40-twins.patch" \
              "$ROOT/patches/0014-playerbot-sunwell.patch" \
              "$ROOT/patches/0011-playerbot-perfmon-hotpath.patch")  # REVERSE order (last patch first)

[[ -d "$PB/.git" ]] || { echo "ERROR: $PB is not a git clone (run setup.sh first)" >&2; exit 1; }
[[ -f "$P0003" ]]   || { echo "ERROR: missing $P0003 (baseline needs 0003's shared-file hunks)" >&2; exit 1; }
[[ -f "$OUT" ]]     || { echo "ERROR: missing $OUT (need its ActionContext.h hunk for the baseline)" >&2; exit 1; }
for p in "${P_LATER_PBAI[@]}"; do
  [[ -f "$p" ]] || { echo "ERROR: missing $(basename "$p") (needed to strip later PlayerbotAI.cpp hunks)" >&2; exit 1; }
done

for f in \
  "$PB/src/Ai/Base/ActionContext.h" \
  "$PB/src/Ai/Base/Actions/MovementActions.cpp" \
  "$PB/src/Ai/Base/Actions/ReleaseSpiritAction.cpp" \
  "$PB/src/Ai/Base/Actions/ReleaseSpiritAction.h" \
  "$PB/src/Ai/Base/Actions/ReviveFromCorpseAction.cpp" \
  "$PB/src/Ai/Base/Strategy/WintergraspSiegeStrategy.cpp" \
  "$PB/src/Ai/Base/Strategy/WintergraspSiegeStrategy.h" \
  "$PB/src/Ai/Base/StrategyContext.h" \
  "$PB/src/Ai/Base/TriggerContext.h" \
  "$PB/src/Bot/Factory/AiFactory.cpp" \
  "$PB/src/Bot/PlayerbotAI.cpp" \
  "$PB/src/Bot/RandomPlayerbotMgr.cpp" \
  "$AC/src/server/game/Battlefield/Zones/BattlefieldWG.cpp"
do
  [[ -f "$f" ]] || { echo "ERROR: missing $f — 0004 edits not present in the fork worktree" >&2; exit 1; }
done

TMP="$(mktemp -d)"
restore() {
  # Restore the worktree exactly as found (0004[+0005] applied) and leave both indexes pristine.
  if [[ -f "$TMP/ActionContext.h" ]];         then cp "$TMP/ActionContext.h"         "$PB/src/Ai/Base/ActionContext.h"; fi
  if [[ -f "$TMP/MovementActions.cpp" ]];     then cp "$TMP/MovementActions.cpp"     "$PB/src/Ai/Base/Actions/MovementActions.cpp"; fi
  if [[ -f "$TMP/ReleaseSpiritAction.cpp" ]]; then cp "$TMP/ReleaseSpiritAction.cpp" "$PB/src/Ai/Base/Actions/ReleaseSpiritAction.cpp"; fi
  if [[ -f "$TMP/ReleaseSpiritAction.h" ]];   then cp "$TMP/ReleaseSpiritAction.h"   "$PB/src/Ai/Base/Actions/ReleaseSpiritAction.h"; fi
  if [[ -f "$TMP/ReviveFromCorpseAction.cpp" ]]; then cp "$TMP/ReviveFromCorpseAction.cpp" "$PB/src/Ai/Base/Actions/ReviveFromCorpseAction.cpp"; fi
  if [[ -f "$TMP/StrategyContext.h" ]];       then cp "$TMP/StrategyContext.h"       "$PB/src/Ai/Base/StrategyContext.h"; fi
  if [[ -f "$TMP/TriggerContext.h" ]];        then cp "$TMP/TriggerContext.h"        "$PB/src/Ai/Base/TriggerContext.h"; fi
  if [[ -f "$TMP/AiFactory.cpp" ]];           then cp "$TMP/AiFactory.cpp"           "$PB/src/Bot/Factory/AiFactory.cpp"; fi
  if [[ -f "$TMP/PlayerbotAI.cpp" ]];         then cp "$TMP/PlayerbotAI.cpp"         "$PB/src/Bot/PlayerbotAI.cpp"; fi
  if [[ -f "$TMP/RandomPlayerbotMgr.cpp" ]];  then cp "$TMP/RandomPlayerbotMgr.cpp"  "$PB/src/Bot/RandomPlayerbotMgr.cpp"; fi
  if [[ -f "$TMP/BattlefieldWG.cpp" ]];       then cp "$TMP/BattlefieldWG.cpp"       "$AC/src/server/game/Battlefield/Zones/BattlefieldWG.cpp"; fi
  git -C "$PB" reset -q -- \
    src/Ai/Base/ActionContext.h \
    src/Ai/Base/Actions/MovementActions.cpp \
    src/Ai/Base/Actions/ReleaseSpiritAction.cpp \
    src/Ai/Base/Actions/ReleaseSpiritAction.h \
    src/Ai/Base/Actions/ReviveFromCorpseAction.cpp \
    src/Ai/Base/Strategy/WintergraspSiegeStrategy.cpp \
    src/Ai/Base/Strategy/WintergraspSiegeStrategy.h \
    src/Ai/Base/StrategyContext.h \
    src/Ai/Base/TriggerContext.h \
    src/Bot/Factory/AiFactory.cpp \
    src/Bot/PlayerbotAI.cpp \
    src/Bot/RandomPlayerbotMgr.cpp 2>/dev/null || true
  git -C "$AC" reset -q -- src/server/game/Battlefield/Zones/BattlefieldWG.cpp 2>/dev/null || true
  rm -rf "$TMP"
}
trap restore EXIT

# 1. Save the current (0004[+0005]) copies of every EXISTING modified file.
cp "$PB/src/Ai/Base/ActionContext.h"                  "$TMP/ActionContext.h"
cp "$PB/src/Ai/Base/Actions/MovementActions.cpp"      "$TMP/MovementActions.cpp"
cp "$PB/src/Ai/Base/Actions/ReleaseSpiritAction.cpp"  "$TMP/ReleaseSpiritAction.cpp"
cp "$PB/src/Ai/Base/Actions/ReleaseSpiritAction.h"    "$TMP/ReleaseSpiritAction.h"
cp "$PB/src/Ai/Base/Actions/ReviveFromCorpseAction.cpp" "$TMP/ReviveFromCorpseAction.cpp"
cp "$PB/src/Ai/Base/StrategyContext.h"                "$TMP/StrategyContext.h"
cp "$PB/src/Ai/Base/TriggerContext.h"                 "$TMP/TriggerContext.h"
cp "$PB/src/Bot/Factory/AiFactory.cpp"                "$TMP/AiFactory.cpp"
cp "$PB/src/Bot/PlayerbotAI.cpp"                      "$TMP/PlayerbotAI.cpp"
cp "$PB/src/Bot/RandomPlayerbotMgr.cpp"               "$TMP/RandomPlayerbotMgr.cpp"
cp "$AC/src/server/game/Battlefield/Zones/BattlefieldWG.cpp" "$TMP/BattlefieldWG.cpp"

# 2. Pristine every existing file (index + worktree back to fork HEAD).
git -C "$PB" checkout -- \
  src/Ai/Base/ActionContext.h \
  src/Ai/Base/Actions/MovementActions.cpp \
  src/Ai/Base/Actions/ReleaseSpiritAction.cpp \
  src/Ai/Base/Actions/ReleaseSpiritAction.h \
  src/Ai/Base/Actions/ReviveFromCorpseAction.cpp \
  src/Ai/Base/StrategyContext.h \
  src/Ai/Base/TriggerContext.h \
  src/Bot/Factory/AiFactory.cpp \
  src/Bot/PlayerbotAI.cpp \
  src/Bot/RandomPlayerbotMgr.cpp
git -C "$AC" checkout -- src/server/game/Battlefield/Zones/BattlefieldWG.cpp

# 3. Build the baseline: apply ONLY 0003's hunks for the three files 0004 shares with 0003.
git -C "$AC" apply \
  --include=modules/mod-playerbots/src/Bot/Factory/AiFactory.cpp \
  --include=modules/mod-playerbots/src/Bot/PlayerbotAI.cpp \
  --include=modules/mod-playerbots/src/Bot/RandomPlayerbotMgr.cpp \
  "$P0003"

# 4. Stage the baseline (index = pristine, +0003 for the shared three).
git -C "$PB" add -- \
  src/Ai/Base/ActionContext.h \
  src/Ai/Base/Actions/MovementActions.cpp \
  src/Ai/Base/Actions/ReleaseSpiritAction.cpp \
  src/Ai/Base/Actions/ReleaseSpiritAction.h \
  src/Ai/Base/Actions/ReviveFromCorpseAction.cpp \
  src/Ai/Base/StrategyContext.h \
  src/Ai/Base/TriggerContext.h \
  src/Bot/Factory/AiFactory.cpp \
  src/Bot/PlayerbotAI.cpp \
  src/Bot/RandomPlayerbotMgr.cpp
git -C "$AC" add -- src/server/game/Battlefield/Zones/BattlefieldWG.cpp

# 5. Put the 0004 target back in the worktree for the diff.
#    - Files 0004-exclusive or 0003-shared: restore the saved current copy.
cp "$TMP/MovementActions.cpp"        "$PB/src/Ai/Base/Actions/MovementActions.cpp"
cp "$TMP/ReleaseSpiritAction.cpp"    "$PB/src/Ai/Base/Actions/ReleaseSpiritAction.cpp"
cp "$TMP/ReleaseSpiritAction.h"      "$PB/src/Ai/Base/Actions/ReleaseSpiritAction.h"
cp "$TMP/ReviveFromCorpseAction.cpp" "$PB/src/Ai/Base/Actions/ReviveFromCorpseAction.cpp"
cp "$TMP/StrategyContext.h"          "$PB/src/Ai/Base/StrategyContext.h"
cp "$TMP/TriggerContext.h"           "$PB/src/Ai/Base/TriggerContext.h"
cp "$TMP/AiFactory.cpp"              "$PB/src/Bot/Factory/AiFactory.cpp"
cp "$TMP/PlayerbotAI.cpp"            "$PB/src/Bot/PlayerbotAI.cpp"
cp "$TMP/RandomPlayerbotMgr.cpp"     "$PB/src/Bot/RandomPlayerbotMgr.cpp"
cp "$TMP/BattlefieldWG.cpp"          "$AC/src/server/game/Battlefield/Zones/BattlefieldWG.cpp"
#    - 5b. PlayerbotAI.cpp only: the saved copy carries 0011/0014/0016 too (all edit this file).
#      REVERSE-apply their PlayerbotAI.cpp hunks (reverse order) to strip them back to
#      pristine+0003+0004, so the diff below emits ONLY 0004's hunks. Without this the regen
#      silently absorbs their registration lines into 0004 (the contamination trap).
for p in "${P_LATER_PBAI[@]}"; do
  git -C "$AC" apply -R --include=modules/mod-playerbots/src/Bot/PlayerbotAI.cpp "$p" \
    || { echo "GATE FAIL: could not reverse $(basename "$p")'s PlayerbotAI.cpp hunks — is it applied/current?" >&2; exit 1; }
done
#    - ActionContext.h: the saved copy carries 0005 too. Rebuild pristine+0004-ONLY from the
#      current patch's own ActionContext.h hunk (unchanged by these fixes).
git -C "$PB" checkout -- src/Ai/Base/ActionContext.h
git -C "$AC" apply --include=modules/mod-playerbots/src/Ai/Base/ActionContext.h "$OUT"

# 6. New files: intent-to-add so the diff emits their full content.
git -C "$PB" add -N -- \
  src/Ai/Base/Strategy/WintergraspSiegeStrategy.cpp \
  src/Ai/Base/Strategy/WintergraspSiegeStrategy.h

# 7. Emit the mod part (fork-root-relative prefixes) then append the core part.
git -C "$PB" diff \
  --src-prefix=a/modules/mod-playerbots/ --dst-prefix=b/modules/mod-playerbots/ -- \
  src/Ai/Base/ActionContext.h \
  src/Ai/Base/Actions/MovementActions.cpp \
  src/Ai/Base/Actions/ReleaseSpiritAction.cpp \
  src/Ai/Base/Actions/ReleaseSpiritAction.h \
  src/Ai/Base/Actions/ReviveFromCorpseAction.cpp \
  src/Ai/Base/Strategy/WintergraspSiegeStrategy.cpp \
  src/Ai/Base/Strategy/WintergraspSiegeStrategy.h \
  src/Ai/Base/StrategyContext.h \
  src/Ai/Base/TriggerContext.h \
  src/Bot/Factory/AiFactory.cpp \
  src/Bot/PlayerbotAI.cpp \
  src/Bot/RandomPlayerbotMgr.cpp \
  > "$TMP/0004.patch"
git -C "$AC" diff -- src/server/game/Battlefield/Zones/BattlefieldWG.cpp >> "$TMP/0004.patch"

# 8. Structural gates (content-agnostic — later tasks add their own greps).
[[ -s "$TMP/0004.patch" ]] || { echo "GATE FAIL: generated patch is empty" >&2; exit 1; }
for needle in "WintergraspSiegeStrategy" "wg siege"; do
  grep -q "$needle" "$TMP/0004.patch" || { echo "GATE FAIL: missing expected content: $needle" >&2; exit 1; }
done
grep -q "^diff --git a/src/server/game/Battlefield/Zones/BattlefieldWG.cpp" "$TMP/0004.patch" \
  || { echo "GATE FAIL: core BattlefieldWG.cpp hunk missing (fork-only regen trap)" >&2; exit 1; }
# Canaries: 0005 (ArenaCoord), 0003 (AcceptWintergraspInvitation), plus the later PlayerbotAI.cpp
# sharers stripped in 5b — 0016 registers `case 531` (AQ40), 0014 `case 580` (Sunwell), 0011
# `PerfMonitorOperation`. Any of these in 0004 means the strip regressed and contamination is back.
for canary in "ArenaCoord" "AcceptWintergraspInvitation" "case 531:" "case 580:" "PerfMonitorOperation"; do
  if grep -q "$canary" "$TMP/0004.patch"; then
    echo "GATE FAIL: contaminated with other-patch content: $canary" >&2; exit 1
  fi
done

cp "$TMP/0004.patch" "$OUT"
echo "OK: wrote $OUT ($(wc -l < "$OUT") lines)"
git -C "$AC" apply --check "$OUT" 2>/dev/null && echo "apply --check: OK (against current worktree baseline)" || true
