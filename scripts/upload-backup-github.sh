#!/usr/bin/env bash
# Upload one backup to a PRIVATE GitHub repository as a release, not Git content.
# Requires gh auth login; BACKUP_GITHUB_REPO=owner/repo. No tokens in this script.
set -euo pipefail
umask 077

REPO="${BACKUP_GITHUB_REPO:?Set BACKUP_GITHUB_REPO to a private owner/repo}"
KEEP="${BACKUP_GITHUB_KEEP:-14}"
GH="${BACKUP_GH_BIN:-gh}"
BUNDLE="${1:?Usage: upload-backup-github.sh /path/to/acore-YYYYmmdd-HHMMSS.tar}"
NAME="$(basename "$BUNDLE")"
if [[ ! "$REPO" =~ ^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$ ]] ||
   [[ ! "$KEEP" =~ ^[1-9][0-9]*$ ]] ||
   [[ ! "$NAME" =~ ^acore-[0-9]{8}-[0-9]{6}\.tar$ ]] || [[ ! -s "$BUNDLE" ]]; then
  echo "ERROR: Invalid repository, retention, or backup filename/file." >&2
  exit 1
fi

# Fail closed: archives contain database account data and plaintext .env secrets.
if [[ "$("$GH" repo view "$REPO" --json isPrivate --jq '.isPrivate')" != true ]]; then
  echo "ERROR: Refusing to upload a backup to a non-private repository." >&2
  exit 1
fi
members="$(tar -tf "$BUNDLE")"
grep -qx 'database.sql.gz' <<< "$members"
grep -qx 'env' <<< "$members"
tar -xOf "$BUNDLE" database.sql.gz | gzip -t

STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT
# Use the basename in the checksum so downloaded assets can be verified together.
(cd "$(dirname "$BUNDLE")" && sha256sum "$NAME") > "$STAGE/$NAME.sha256"
TAG="${NAME%.tar}"
echo "[$(date)] Uploading $NAME to private repository $REPO..."
# A partial upload remains a draft for diagnosis; never overwrite an existing tag.
"$GH" release create "$TAG" "$BUNDLE" "$STAGE/$NAME.sha256" \
  --repo "$REPO" --draft --title "AzerothCore backup ${TAG#acore-}" \
  --notes 'Full dump of the four AzerothCore databases plus .env. Contains secrets; keep this repository private. Verify with sha256sum -c before restoring.'

for asset in "$BUNDLE" "$STAGE/$NAME.sha256"; do
  asset_name="$(basename "$asset")"
  remote_size="$("$GH" release view "$TAG" --repo "$REPO" --json assets \
    --jq ".assets[] | select(.name == \"$asset_name\") | .size")"
  if [[ "$remote_size" != "$(stat -c %s "$asset")" ]]; then
    echo "ERROR: Uploaded asset size mismatch; leaving $TAG as a draft." >&2
    exit 1
  fi
done
"$GH" release edit "$TAG" --repo "$REPO" --draft=false --latest

# Only prune our timestamped, published backup releases after a successful upload.
# Capture the command output first: API failures must not be hidden by a pipeline.
old_tags="$("$GH" release list --repo "$REPO" --limit 1000 --json tagName,isDraft \
  --jq "[.[] | select(.isDraft == false) | .tagName | select(test(\"^acore-[0-9]{8}-[0-9]{6}$\"))] | sort | reverse | .[$KEEP:] | .[]")"
while IFS= read -r old_tag; do
  [[ -n "$old_tag" ]] || continue
  # Extra guard against deleting unrelated tags even if listing output is wrong.
  if [[ ! "$old_tag" =~ ^acore-[0-9]{8}-[0-9]{6}$ ]] || [[ "$old_tag" == "$TAG" ]]; then
    echo "ERROR: Refusing to prune unexpected/current tag: $old_tag" >&2
    exit 1
  fi
  "$GH" release delete "$old_tag" --repo "$REPO" --yes --cleanup-tag
done <<< "$old_tags"
echo "[$(date)] GitHub backup OK: https://github.com/$REPO/releases/tag/$TAG"
