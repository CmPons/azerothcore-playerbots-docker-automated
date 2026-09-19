#!/usr/bin/env bash
# Show every workspace repository; optionally verify our forks over the network.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
case "${1:-}" in
  --remote|"") ;;
  *) echo "Usage: $0 [--remote]" >&2; exit 2 ;;
esac
[[ $# -le 1 ]] || exit 2
failed=0
repos=("$ROOT" "$ROOT/azerothcore-wotlk")
for dir in "$ROOT"/azerothcore-wotlk/modules/*; do
  [[ ! -e "$dir/.git" ]] || repos+=("$dir")
done
for dir in "${repos[@]}"; do
  printf '\n=== %s ===\n' "${dir#"$ROOT"/}"
  if [[ ! -e "$dir/.git" ]]; then
    echo 'MISSING'; failed=1; continue
  fi
  git -C "$dir" status --short --branch --untracked-files=all
  head="$(git -C "$dir" rev-parse HEAD)"
  echo "HEAD: $head"
  [[ -z "$(git -C "$dir" status --porcelain --untracked-files=all)" ]] || failed=1
  if [[ "$dir" != "$ROOT" ]]; then
    pin="$(awk -v r="$(basename "$dir")" '$1==r {print $2; exit}' "$ROOT/repo-pins.txt")"
    if [[ "$pin" != "$head" ]]; then echo "PIN MISMATCH: ${pin:-missing}"; failed=1; fi
  fi
  if git -C "$dir" remote get-url fork >/dev/null 2>&1; then
    branch=local-playerbot
    [[ "$dir" != "$ROOT" ]] || branch=main
    echo "Fork: $(git -C "$dir" remote get-url --push fork) ($branch)"
    if [[ "${1:-}" == --remote ]]; then
      if remote="$(git -C "$dir" ls-remote --exit-code fork "refs/heads/$branch")" && [[ "${remote%%$'\t'*}" == "$head" ]]; then
        echo 'Published: YES (remote tip verified)'
      else
        echo 'Published: NO / remote check failed'; failed=1
      fi
    fi
  fi
done
exit "$failed"
