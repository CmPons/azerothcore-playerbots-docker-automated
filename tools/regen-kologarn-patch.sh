#!/usr/bin/env bash
# regen-kologarn-patch.sh — re-cut patches/0006-playerbot-kologarn-eyebeam.patch from the fork
# working tree at azerothcore-wotlk/modules/mod-playerbots.
#
# NOT baseline-aware (unlike regen-arena/regen-wg-siege): patch 0006 touches ONLY files under
# src/Ai/Raid/Uld/, and NO other patch (0003/0004/0005) touches that tree. So a plain scoped
# `git diff` of Raid/Uld from the fork clone emits exactly 0006's hunks — no shared-file baseline
# gymnastics needed. The prefix flags make the patch apply from the fork root, matching how
# setup.sh's apply_patches invokes `git -C "$AC_DIR" apply`.
#
# LITERAL PATHS ONLY on every git line — the shell does not word-split an unquoted $var, so a
# `for f in $FILES` loop would cut an empty patch (this trap has bitten the WG/publish recipes).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
AC="$ROOT/azerothcore-wotlk"
PB="$AC/modules/mod-playerbots"
OUT="$ROOT/patches/0006-playerbot-kologarn-eyebeam.patch"

[[ -d "$PB/.git" ]] || { echo "ERROR: $PB is not a git clone (run setup.sh first)" >&2; exit 1; }

# All five 0006 files must be present with the edits applied in the worktree.
for f in \
  "$PB/src/Ai/Raid/Uld/UldActions.cpp" \
  "$PB/src/Ai/Raid/Uld/UldActions.h" \
  "$PB/src/Ai/Raid/Uld/UldTriggers.cpp" \
  "$PB/src/Ai/Raid/Uld/Util/UldBossHelper.h" \
  "$PB/src/Ai/Raid/Uld/Util/UldGeometry.h"
do
  [[ -f "$f" ]] || { echo "ERROR: missing $f — Kologarn edits not present in the fork worktree" >&2; exit 1; }
done
grep -q "KologarnShouldHoldRightArm" "$PB/src/Ai/Raid/Uld/Util/UldBossHelper.h" \
  || { echo "ERROR: UldBossHelper.h missing KologarnShouldHoldRightArm — 0006 not applied?" >&2; exit 1; }

# New file rides along via intent-to-add so it appears in the diff; unstage it afterwards so the
# fork index is left exactly as found.
git -C "$PB" add -N -- src/Ai/Raid/Uld/Util/UldGeometry.h
git -C "$PB" diff \
  --src-prefix=a/modules/mod-playerbots/ \
  --dst-prefix=b/modules/mod-playerbots/ \
  -- src/Ai/Raid/Uld \
  > "$OUT"
git -C "$PB" reset -q -- src/Ai/Raid/Uld/Util/UldGeometry.h

# Gates — a bad patch here plus a setup.sh fork reset would orphan the Kologarn work.
[[ -s "$OUT" ]] || { echo "GATE FAIL: generated patch is empty" >&2; exit 1; }
[[ "$(grep -c '^diff --git' "$OUT")" -eq 5 ]] || { echo "GATE FAIL: expected 5 files in the patch" >&2; exit 1; }
for needle in "UldGeometry" "KologarnFindArm" "KologarnShouldHoldRightArm" "KologarnCollectEyes" "IsOverKologarnPit"; do
  grep -q "$needle" "$OUT" || { echo "GATE FAIL: patch missing expected content: $needle" >&2; exit 1; }
done
if grep -q "KolEyebeam" "$OUT"; then
  echo "GATE FAIL: patch still contains temporary [KolEyebeam] debug logging" >&2; exit 1
fi

echo "OK: wrote $OUT ($(wc -l < "$OUT") lines)"
git -C "$AC" apply --stat "$OUT" | sed 's/^/    /'
