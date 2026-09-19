#!/usr/bin/env bash
# Prepare published, pinned sources and build worldserver WITHOUT deploying it.
# No hard resets, cleaning, patch replay, volume changes or service restarts.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
case "${1:-}" in
  --sources-only|"") ;;
  *) echo "Usage: $0 [--sources-only]" >&2; exit 2 ;;
esac
[[ $# -le 1 ]] || { echo "Too many arguments" >&2; exit 2; }

"$ROOT/setup.sh" --sources-only
[[ "${1:-}" != --sources-only ]] || exit 0

cd "$ROOT/azerothcore-wotlk"
docker compose build ac-worldserver
printf '\nWorldserver image built. Running services are UNCHANGED.\n'
printf 'Deployment/restart requires separate explicit authorization and preservation checks.\n'
