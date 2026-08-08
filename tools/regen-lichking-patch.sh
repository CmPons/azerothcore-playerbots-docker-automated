#!/usr/bin/env bash
# regen-lichking-patch.sh — re-cut patches/0007-playerbot-lichking-phase3.patch from the
# fork working tree at azerothcore-wotlk/modules/mod-playerbots.
#
# NOT baseline-aware (like regen-kologarn, unlike regen-arena/regen-wg-siege): patch 0007
# touches ONLY files under src/Ai/Raid/ICC/, and NO other patch (0001-0006) touches that
# tree. A plain scoped `git diff` of Raid/ICC therefore emits exactly 0007's hunks — no
# shared-file baseline gymnastics needed. The prefix flags make the patch apply from the
# fork root, matching how setup.sh's apply_patches invokes `git -C "$AC_DIR" apply`.
#
# LITERAL PATHS ONLY on every git line — the shell does not word-split an unquoted $var,
# so a `for f in $FILES` loop would cut an empty patch (this trap has bitten the WG and
# publish recipes).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
AC="$ROOT/azerothcore-wotlk"
PB="$AC/modules/mod-playerbots"
OUT="$ROOT/patches/0007-playerbot-lichking-phase3.patch"

[[ -d "$PB/.git" ]] || { echo "ERROR: $PB is not a git clone (run setup.sh first)" >&2; exit 1; }

[[ -f "$PB/src/Ai/Raid/ICC/Action/ICCActions_LK.cpp" ]] \
  || { echo "ERROR: missing ICCActions_LK.cpp — Lich King edits not in the fork worktree" >&2; exit 1; }
grep -q "IsSpiritArmed" "$PB/src/Ai/Raid/ICC/Action/ICCActions_LK.cpp" \
  || { echo "ERROR: ICCActions_LK.cpp missing IsSpiritArmed — 0007 not applied?" >&2; exit 1; }

git -C "$PB" diff \
  --src-prefix=a/modules/mod-playerbots/ \
  --dst-prefix=b/modules/mod-playerbots/ \
  -- src/Ai/Raid/ICC \
  > "$OUT"

# Gates — a bad patch here plus a setup.sh fork reset would orphan the work.
[[ -s "$OUT" ]] || { echo "GATE FAIL: generated patch is empty" >&2; exit 1; }
[[ "$(grep -c '^diff --git' "$OUT")" -eq 1 ]] || { echo "GATE FAIL: expected 1 file in the patch" >&2; exit 1; }
for needle in "IsSpiritArmed" "s_healerPause" "dormantSeen" "PACE_GAP" "ArmedSpiritNear" "DefileSafeAt"; do
  grep -q "$needle" "$OUT" || { echo "GATE FAIL: patch missing expected content: $needle" >&2; exit 1; }
done
# The withdrawn rev-1 mechanisms must never reappear in this patch.
for banned in "dormantWave" "HazardAwareStep" "SETTLE_MS" "waveSettling"; do
  if grep -q "$banned" "$OUT"; then
    echo "GATE FAIL: patch contains withdrawn rev-1 mechanism: $banned" >&2; exit 1
  fi
done
if grep -q "LKPhase3Debug" "$OUT"; then
  echo "GATE FAIL: patch still contains temporary [LKPhase3Debug] debug logging" >&2; exit 1
fi

echo "OK: wrote $OUT ($(wc -l < "$OUT") lines)"
git -C "$AC" apply --stat "$OUT" | sed 's/^/    /'
