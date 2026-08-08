#!/usr/bin/env bash
# regen-arena-patch.sh — re-cut patches/0005-playerbot-arena-coordination.patch from the
# fork working tree at azerothcore-wotlk/modules/mod-playerbots.
#
# WHY THIS IS BASELINE-AWARE: the fork's patches (0003/0004) are applied UNSTAGED, so a naive
# `git diff` of a modified file emits hunks from EVERY applied patch, not just 0005. Concretely,
# ActionContext.h is also touched by patch 0004 ("wg siege") — embedding that hunk into 0005
# would make setup.sh's ordered apply (0003 -> 0004 -> 0005) fail. So for the modified files we
# diff against a baseline of pristine+0004 using the git index:
#   1. save the current (0005-bearing) copies of the modified files
#   2. checkout -- (pristine), re-apply ONLY the earlier-patch hunks for shared files
#      (0004 --include=ActionContext.h; 0003 touches none of the 0005 files)
#   3. `git add` the baseline files (index = baseline), copy the saved 0005 copies back
#   4. `git diff` (index vs worktree) == exactly the 0005 hunks, correct headers
#   5. `git reset` the paths so the fork index is left pristine, worktree left at 0005
# New files ride along via `git add -N` (intent-to-add) in the same diff.
#
# LITERAL PATHS ONLY on every git/cp line — zsh does not word-split unquoted vars (a `$FILES`
# loop silently copies nothing and cuts an empty patch; this trap has bitten twice before).
#
# Re-runnable: works from the reviewed-working-tree state AND from a post-setup.sh state
# (patch 0005 applied). Leaves the fork tree and index exactly as found.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
AC="$ROOT/azerothcore-wotlk"
PB="$AC/modules/mod-playerbots"
OUT="$ROOT/patches/0005-playerbot-arena-coordination.patch"
P0004="$ROOT/patches/0004-playerbot-wintergrasp-siege.patch"

[[ -d "$PB/.git" ]] || { echo "ERROR: $PB is not a git clone (run setup.sh first)" >&2; exit 1; }
[[ -f "$P0004" ]]   || { echo "ERROR: missing $P0004 (baseline needs 0004's ActionContext.h hunks)" >&2; exit 1; }

# All eight 0005 files must be present in the worktree (i.e. 0005 edits are applied).
for f in \
  "$PB/src/Ai/Base/ValueContext.h" \
  "$PB/src/Ai/Base/ActionContext.h" \
  "$PB/src/Ai/Base/Actions/BattleGroundTactics.cpp" \
  "$PB/src/Ai/Base/Strategy/BattlegroundStrategy.cpp" \
  "$PB/src/Ai/Base/Value/ArenaCoordValues.h" \
  "$PB/src/Ai/Base/Value/ArenaCoordValues.cpp" \
  "$PB/src/Ai/Base/Actions/ArenaCoordActions.h" \
  "$PB/src/Ai/Base/Actions/ArenaCoordActions.cpp"
do
  [[ -f "$f" ]] || { echo "ERROR: missing $f — arena edits not present in the fork worktree" >&2; exit 1; }
done
grep -q "ArenaCoordValues" "$PB/src/Ai/Base/ValueContext.h" \
  || { echo "ERROR: ValueContext.h has no ArenaCoordValues include — 0005 not applied?" >&2; exit 1; }

TMP="$(mktemp -d)"
restore() {
  # Always leave the fork exactly as found: 0005 worktree state, pristine index.
  # (`if` not `&&` — under set -e a failing `[[ ]] && cmd` compound aborts the trap.)
  if [[ -f "$TMP/ValueContext.h" ]];           then cp "$TMP/ValueContext.h"           "$PB/src/Ai/Base/ValueContext.h"; fi
  if [[ -f "$TMP/ActionContext.h" ]];          then cp "$TMP/ActionContext.h"          "$PB/src/Ai/Base/ActionContext.h"; fi
  if [[ -f "$TMP/BattleGroundTactics.cpp" ]];  then cp "$TMP/BattleGroundTactics.cpp"  "$PB/src/Ai/Base/Actions/BattleGroundTactics.cpp"; fi
  if [[ -f "$TMP/BattlegroundStrategy.cpp" ]]; then cp "$TMP/BattlegroundStrategy.cpp" "$PB/src/Ai/Base/Strategy/BattlegroundStrategy.cpp"; fi
  git -C "$PB" reset -q -- \
    src/Ai/Base/ValueContext.h \
    src/Ai/Base/ActionContext.h \
    src/Ai/Base/Actions/BattleGroundTactics.cpp \
    src/Ai/Base/Strategy/BattlegroundStrategy.cpp \
    src/Ai/Base/Value/ArenaCoordValues.h \
    src/Ai/Base/Value/ArenaCoordValues.cpp \
    src/Ai/Base/Actions/ArenaCoordActions.h \
    src/Ai/Base/Actions/ArenaCoordActions.cpp 2>/dev/null || true
  rm -rf "$TMP"
}
trap restore EXIT

# 1. Save the current (0005-bearing) modified files.
cp "$PB/src/Ai/Base/ValueContext.h"                     "$TMP/ValueContext.h"
cp "$PB/src/Ai/Base/ActionContext.h"                    "$TMP/ActionContext.h"
cp "$PB/src/Ai/Base/Actions/BattleGroundTactics.cpp"    "$TMP/BattleGroundTactics.cpp"
cp "$PB/src/Ai/Base/Strategy/BattlegroundStrategy.cpp"  "$TMP/BattlegroundStrategy.cpp"

# 2. Pristine, then rebuild the baseline: only ActionContext.h is shared with an earlier
#    patch (0004). 0003 touches none of the 0005 files (verified against patches/0003-*).
git -C "$PB" checkout -- \
  src/Ai/Base/ValueContext.h \
  src/Ai/Base/ActionContext.h \
  src/Ai/Base/Actions/BattleGroundTactics.cpp \
  src/Ai/Base/Strategy/BattlegroundStrategy.cpp
git -C "$AC" apply --include=modules/mod-playerbots/src/Ai/Base/ActionContext.h "$P0004"

# 3. Stage the baseline (index = pristine+0004 for these paths).
git -C "$PB" add -- \
  src/Ai/Base/ValueContext.h \
  src/Ai/Base/ActionContext.h \
  src/Ai/Base/Actions/BattleGroundTactics.cpp \
  src/Ai/Base/Strategy/BattlegroundStrategy.cpp

# 4. Restore the 0005 worktree state; mark new files intent-to-add.
cp "$TMP/ValueContext.h"          "$PB/src/Ai/Base/ValueContext.h"
cp "$TMP/ActionContext.h"         "$PB/src/Ai/Base/ActionContext.h"
cp "$TMP/BattleGroundTactics.cpp" "$PB/src/Ai/Base/Actions/BattleGroundTactics.cpp"
cp "$TMP/BattlegroundStrategy.cpp" "$PB/src/Ai/Base/Strategy/BattlegroundStrategy.cpp"
git -C "$PB" add -N -- \
  src/Ai/Base/Value/ArenaCoordValues.h \
  src/Ai/Base/Value/ArenaCoordValues.cpp \
  src/Ai/Base/Actions/ArenaCoordActions.h \
  src/Ai/Base/Actions/ArenaCoordActions.cpp

# 5. Index(baseline) vs worktree(0005) == exactly the 0005 hunks. Prefixes make the patch
#    apply from the fork root, matching how setup.sh's apply_patches invokes git apply.
git -C "$PB" diff \
  --src-prefix=a/modules/mod-playerbots/ \
  --dst-prefix=b/modules/mod-playerbots/ \
  -- \
  src/Ai/Base/ValueContext.h \
  src/Ai/Base/ActionContext.h \
  src/Ai/Base/Actions/BattleGroundTactics.cpp \
  src/Ai/Base/Strategy/BattlegroundStrategy.cpp \
  src/Ai/Base/Value/ArenaCoordValues.h \
  src/Ai/Base/Value/ArenaCoordValues.cpp \
  src/Ai/Base/Actions/ArenaCoordActions.h \
  src/Ai/Base/Actions/ArenaCoordActions.cpp \
  > "$TMP/0005.patch"

# (index/worktree restoration happens in the EXIT trap)

# 6. Gates — a bad patch here plus a setup.sh reset would orphan the arena work.
[[ -s "$TMP/0005.patch" ]] || { echo "GATE FAIL: generated patch is empty" >&2; exit 1; }
for needle in "ArenaCoordValues" "ArenaCoordActions" "arena kill target" "arena pvp trinket"; do
  grep -q "$needle" "$TMP/0005.patch" || { echo "GATE FAIL: patch missing expected content: $needle" >&2; exit 1; }
done
for canary in "wg siege" "Wintergrasp"; do
  if grep -q "$canary" "$TMP/0005.patch"; then
    echo "GATE FAIL: patch contaminated with earlier-patch content: $canary" >&2; exit 1
  fi
done

cp "$TMP/0005.patch" "$OUT"
echo "OK: wrote $OUT ($(wc -l < "$OUT") lines)"
git -C "$PB" apply --stat "$OUT" | sed 's/^/    /'
