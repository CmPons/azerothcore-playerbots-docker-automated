#!/usr/bin/env bash
# regen-perfmon-patch.sh — re-cut patches/0011-playerbot-perfmon-hotpath.patch from the fork
# working tree at azerothcore-wotlk/modules/mod-playerbots.
#
# WHY THIS IS BASELINE-AWARE: src/Bot/PlayerbotAI.cpp is ALSO touched by patches 0003 and 0004
# (Wintergrasp), which sit in the worktree UNSTAGED — so a naive scoped `git diff` would embed
# their hunks and make setup.sh's ordered apply (0003 -> 0004 -> ... -> 0011) fail. Same recipe
# as regen-arena-patch.sh:
#   1. save the current (0011-bearing) PlayerbotAI.cpp
#   2. checkout -- (pristine), re-apply ONLY 0003's and 0004's PlayerbotAI.cpp hunks
#   3. `git add` the baseline file (index = pristine+0003+0004)
#   4. copy the saved 0011 copy back; `git diff` (index vs worktree) == exactly the 0011 hunk
#   5. `git reset` the path so the fork index is left pristine, worktree left at 0011
#
# WHAT 0011 IS: PlayerbotAI::UpdateAIInternal built its PerfMonitor label (a WorldPosition +
# std::to_string + string concat = a heap allocation) unconditionally, per bot per AI tick, even
# with PerfMonEnabled=0 — at thousands of bots that is ~100k+ needless allocations/second on the
# map threads. 0011 gates the label construction on sPlayerbotAIConfig.perfMonEnabled.
#
# LITERAL PATHS ONLY on every git/cp line — zsh does not word-split unquoted vars (a `$FILES`
# loop silently copies nothing and cuts an empty patch; this trap has bitten twice before).
#
# Re-runnable: works from the reviewed-working-tree state AND from a post-setup.sh state
# (patch 0011 applied). Leaves the fork tree and index exactly as found.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
AC="$ROOT/azerothcore-wotlk"
PB="$AC/modules/mod-playerbots"
OUT="$ROOT/patches/0011-playerbot-perfmon-hotpath.patch"
P0003="$ROOT/patches/0003-playerbot-wintergrasp.patch"
P0004="$ROOT/patches/0004-playerbot-wintergrasp-siege.patch"

[[ -d "$PB/.git" ]] || { echo "ERROR: $PB is not a git clone (run setup.sh first)" >&2; exit 1; }
[[ -f "$P0003" ]]   || { echo "ERROR: missing $P0003 (baseline needs 0003's PlayerbotAI.cpp hunks)" >&2; exit 1; }
[[ -f "$P0004" ]]   || { echo "ERROR: missing $P0004 (baseline needs 0004's PlayerbotAI.cpp hunks)" >&2; exit 1; }

# The edit must actually be present in the worktree.
grep -q "if (sPlayerbotAIConfig.perfMonEnabled)" "$PB/src/Bot/PlayerbotAI.cpp" \
  || { echo "ERROR: PlayerbotAI.cpp has no perfMonEnabled gate — 0011 not applied?" >&2; exit 1; }

TMP="$(mktemp -d)"
restore() {
  # Always leave the fork exactly as found: 0011 worktree state, pristine index.
  # (`if` not `&&` — under set -e a failing `[[ ]] && cmd` compound aborts the trap.)
  if [[ -f "$TMP/PlayerbotAI.cpp" ]]; then cp "$TMP/PlayerbotAI.cpp" "$PB/src/Bot/PlayerbotAI.cpp"; fi
  git -C "$PB" reset -q -- src/Bot/PlayerbotAI.cpp 2>/dev/null || true
  rm -rf "$TMP"
}
trap restore EXIT

# 1. Save the current (0011-bearing) copy.
cp "$PB/src/Bot/PlayerbotAI.cpp" "$TMP/PlayerbotAI.cpp"

# 2. Pristine, then rebuild the baseline: PlayerbotAI.cpp is shared with 0003 AND 0004 (both
#    carry hunks for it), applied in patch order.
git -C "$PB" checkout -- src/Bot/PlayerbotAI.cpp
git -C "$AC" apply --include=modules/mod-playerbots/src/Bot/PlayerbotAI.cpp "$P0003"
git -C "$AC" apply --include=modules/mod-playerbots/src/Bot/PlayerbotAI.cpp "$P0004"

# 3. Stage the baseline (index = pristine+0003+0004 for this path).
git -C "$PB" add -- src/Bot/PlayerbotAI.cpp

# 4. Restore the 0011 worktree state.
cp "$TMP/PlayerbotAI.cpp" "$PB/src/Bot/PlayerbotAI.cpp"

# 5. Index(baseline) vs worktree(0011) == exactly the 0011 hunk. Prefixes make the patch
#    apply from the fork root, matching how setup.sh's apply_patches invokes git apply.
git -C "$PB" diff \
  --src-prefix=a/modules/mod-playerbots/ \
  --dst-prefix=b/modules/mod-playerbots/ \
  -- src/Bot/PlayerbotAI.cpp \
  > "$TMP/0011.patch"

# (index/worktree restoration happens in the EXIT trap)

# 6. Gates.
[[ -s "$TMP/0011.patch" ]] || { echo "GATE FAIL: generated patch is empty" >&2; exit 1; }
[[ "$(grep -c '^diff --git' "$TMP/0011.patch")" -eq 1 ]] \
  || { echo "GATE FAIL: expected exactly 1 file in the patch" >&2; exit 1; }
for needle in "perfMonEnabled" "PerfMonitorOperation\* pmo = nullptr"; do
  grep -q -- "$needle" "$TMP/0011.patch" || { echo "GATE FAIL: patch missing expected content: $needle" >&2; exit 1; }
done
for canary in "Wintergrasp" "wg siege" "BattlefieldWG"; do
  if grep -qi -- "$canary" "$TMP/0011.patch"; then
    echo "GATE FAIL: patch contaminated with earlier-patch content: $canary" >&2; exit 1
  fi
done

cp "$TMP/0011.patch" "$OUT"
echo "OK: wrote $OUT ($(wc -l < "$OUT") lines)"
git -C "$PB" apply --stat "$OUT" | sed 's/^/    /'
