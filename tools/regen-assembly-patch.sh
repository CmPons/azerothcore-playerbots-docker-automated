#!/usr/bin/env bash
# regen-assembly-patch.sh — re-cut the Ulduar Assembly of Iron patch from the fork working tree.
#
# OUTPUT GOES TO patches/wip/, NOT patches/. That is deliberate and is the whole reason this script
# exists in this form: the Assembly work is implemented and reviewed but has NEVER been live-raid
# verified, and `apply_patches` in setup.sh/update.sh globs `patches/*.patch` NON-recursively — so a
# patch parked in patches/wip/ is preserved by git and ignored by every build. Promote it with a
# single `git mv patches/wip/<name> patches/` once a live kill confirms it.
#
# It was found by a clean-room audit: the fork worktree carried ~580 lines of Assembly changes that no
# patch captured, so the next setup.sh/update.sh fork reset would have destroyed them silently. They
# had been riding along in every dev build for two weeks.
#
# BASELINE-AWARE for src/Ai/Raid/Uld: patch 0006 (Kologarn) touches five files in that same tree, so a
# naive scoped `git diff` would embed 0006's hunks into this patch. The baseline is built in the
# mod-playerbots index as pristine+0006 and diffed against the worktree, the same mechanism as
# regen-arena-patch.sh and regen-sunwell-patch.sh.
#
# TWO CONVENTIONS IN ONE PATCH, like 0004: the module half needs the `modules/mod-playerbots/` prefix
# rewritten in so it applies from the fork root, while the core half (boss_assembly_of_iron.cpp) lives
# in the fork-root repo and takes a PLAIN diff with no prefix rewriting. The core hunk is appended, and
# a gate below fails if it is missing — a module-only regen would silently drop it.
#
# LITERAL PATHS ONLY on every git/cp line — the shell does not word-split an unquoted $var, so a
# `for f in $FILES` loop cuts an empty patch (this trap has bitten the WG and publish recipes).
#
# Re-runnable; leaves the fork worktree and index exactly as found.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
AC="$ROOT/azerothcore-wotlk"
PB="$AC/modules/mod-playerbots"
OUT="$ROOT/patches/wip/0015-playerbot-assembly-of-iron.patch"
P0006="$ROOT/patches/0006-playerbot-kologarn-eyebeam.patch"
CORE="src/server/scripts/Northrend/Ulduar/Ulduar/boss_assembly_of_iron.cpp"

[[ -d "$PB/.git" ]] || { echo "ERROR: $PB is not a git clone (run setup.sh first)" >&2; exit 1; }
[[ -f "$P0006" ]] || { echo "ERROR: baseline patch 0006 missing from patches/" >&2; exit 1; }

# The Assembly work must actually be present, or this would happily cut an empty/backwards patch over
# the top of a good one.
grep -q "IronAssemblyRuneOfDeathAction" "$PB/src/Ai/Raid/Uld/UldActions.h" \
  || { echo "ERROR: UldActions.h has no IronAssemblyRuneOfDeathAction — Assembly edits not in the worktree" >&2; exit 1; }
grep -q "GetAssemblyMember" "$PB/src/Ai/Raid/Uld/UldTriggers.cpp" \
  || { echo "ERROR: UldTriggers.cpp has no GetAssemblyMember — Assembly edits not in the worktree" >&2; exit 1; }

TMP="$(mktemp -d)"

# Restore is unconditional and copies the WHOLE Uld tree back, because step 2 below runs a
# `git checkout --` over uncommitted work that exists nowhere else. If this trap ever fails to fire the
# work is gone: git stored no blob for it.
restore () {
  if [[ -d "$TMP/Uld" ]]; then
    rm -rf "$PB/src/Ai/Raid/Uld"
    cp -a "$TMP/Uld" "$PB/src/Ai/Raid/Uld"
  fi
  if [[ -f "$TMP/boss_assembly_of_iron.cpp" ]]; then
    cp -a "$TMP/boss_assembly_of_iron.cpp" "$AC/$CORE"
  fi
  git -C "$PB" reset -q -- src/Ai/Raid/Uld 2>/dev/null || true
  rm -rf "$TMP"
}
trap restore EXIT

# 1. Save the Assembly-bearing worktree.
cp -a "$PB/src/Ai/Raid/Uld" "$TMP/Uld"
cp -a "$AC/$CORE" "$TMP/boss_assembly_of_iron.cpp"
[[ -f "$TMP/Uld/UldActions.h" && -f "$TMP/boss_assembly_of_iron.cpp" ]] \
  || { echo "ERROR: backup of the worktree failed — refusing to continue" >&2; exit 1; }

# 2. Pristine, then rebuild the baseline: pristine + 0006, restricted to the Uld tree. 0006 touches
#    nothing else, so applying it whole is exactly the baseline.
#
#    The `clean` is REQUIRED and not defensive tidying: 0006 CREATES Util/UldGeometry.h, and a
#    `checkout --` does not remove untracked files, so a second run would hit
#    "already exists in working directory" and abort. setup.sh does the same thing for the same
#    reason before apply_patches. Safe here because step 1 backed up the whole tree and `restore`
#    replaces it wholesale.
git -C "$PB" checkout -- src/Ai/Raid/Uld
git -C "$PB" clean -fdq -- src/Ai/Raid/Uld
git -C "$AC" apply "$P0006"

# 3. Stage the baseline (index = pristine + 0006 for the Uld tree).
git -C "$PB" add -- src/Ai/Raid/Uld

# 4. Restore the Assembly worktree over the top.
rm -rf "$PB/src/Ai/Raid/Uld"
cp -a "$TMP/Uld" "$PB/src/Ai/Raid/Uld"

# 5. Index(baseline) vs worktree(Assembly) == exactly the Assembly hunks, 0006 excluded.
mkdir -p "$ROOT/patches/wip"
git -C "$PB" diff \
  --src-prefix=a/modules/mod-playerbots/ \
  --dst-prefix=b/modules/mod-playerbots/ \
  -- src/Ai/Raid/Uld \
  > "$TMP/0015.patch"

# 6. Append the CORE half with a PLAIN diff — no prefix rewriting, because `a/src/...` already
#    resolves from the fork root, which is where apply_patches runs `git -C "$AC_DIR" apply`.
git -C "$AC" diff -- "$CORE" >> "$TMP/0015.patch"

# 7. Gates.
[[ -s "$TMP/0015.patch" ]] || { echo "GATE FAIL: generated patch is empty" >&2; exit 1; }
grep -q "^diff --git a/$CORE" "$TMP/0015.patch" \
  || { echo "GATE FAIL: core boss_assembly_of_iron.cpp hunk missing (module-only regen trap)" >&2; exit 1; }
for needle in "IronAssemblyRuneOfDeathAction" "IronAssemblyStaticDisruptionAction" \
              "IronAssemblyMarkKillOrderAction" "IronAssemblyOffTankTauntAction" \
              "IronAssemblyOffTankSeparateAction" "GetAssemblyMember" \
              "iron assembly rune of death action"; do
  grep -q "$needle" "$TMP/0015.patch" || { echo "GATE FAIL: patch missing: $needle" >&2; exit 1; }
done
# 0006 content must NOT leak in — that is what the baseline exists to prevent.
for banned in "KologarnShouldHoldRightArm" "KologarnCollectEyes" "IsOverKologarnPit"; do
  if grep -q "$banned" "$TMP/0015.patch"; then
    echo "GATE FAIL: patch contaminated with patch 0006 content: $banned" >&2; exit 1
  fi
done

cp "$TMP/0015.patch" "$OUT"
echo "OK: wrote $OUT ($(wc -l < "$OUT") lines)"
echo "    NOTE: parked in patches/wip/ — NOT applied by setup.sh/update.sh until promoted."
git -C "$AC" apply --stat "$OUT" | sed 's/^/    /'
