#!/usr/bin/env bash
# Shared source-only operations. Callers supply ROOT, AC_DIR and PINS_FILE.
# Never reset, clean, stash, replay patches, or interrupt running services.

source_error() { echo "ERROR: $*" >&2; return 1; }

pin_for() {
  awk -v r="$1" '!/^[[:space:]]*#/ && NF>=2 && $1==r {print $2; exit}' "$PINS_FILE"
}

require_clean_repo() {
  local dir="$1" status
  status="$(git -C "$dir" status --porcelain --untracked-files=all)" || return 1
  if [[ -n "$status" ]]; then
    source_error "Uncommitted/untracked work in $dir. Commit and push it first; nothing will be discarded."
    return 1
  fi
}

sync_source_repo() {
  local dir="$1" url="$2" branch="$3" pin current
  pin="$(pin_for "$(basename "$dir")")"
  [[ "$pin" =~ ^[0-9a-f]{40}$ ]] || { source_error "Missing/invalid pin for $dir in $PINS_FILE"; return 1; }
  if [[ ! -e "$dir/.git" ]]; then
    if [[ -e "$dir" ]] && [[ -n "$(ls -A "$dir")" ]]; then
      source_error "Refusing to replace nonempty directory $dir"; return 1
    fi
    git init -q "$dir" || return 1
    git -C "$dir" remote add origin "$url" || return 1
    git -C "$dir" fetch origin "$pin" || return 1
    git -C "$dir" switch -c "$branch" --no-track FETCH_HEAD || return 1
    if [[ "$url" == https://github.com/CmPons/* ]]; then
      git -C "$dir" remote add fork "$url" || return 1
      git -C "$dir" config remote.pushDefault fork || return 1
      git -C "$dir" config push.default current || return 1
    fi
    return 0
  fi

  require_clean_repo "$dir" || return 1
  current="$(git -C "$dir" rev-parse HEAD)" || return 1
  [[ "$current" != "$pin" ]] || return 0
  git -C "$dir" fetch "$url" "$pin" || return 1
  if ! git -C "$dir" merge-base --is-ancestor HEAD "$pin"; then
    source_error "$dir is ahead of or diverges from its pin. Publish/reconcile it and update repo-pins.txt; refusing to rewind."
    return 1
  fi
  git -C "$dir" merge --ff-only "$pin"
}

sync_local_module() {
  local name="$1" src="$ROOT/modules/$1" dst="$AC_DIR/modules/$1"
  [[ -d "$src" ]] || { source_error "Missing root module $src"; return 1; }
  if [[ -e "$dst" || -L "$dst" ]]; then
    if [[ -L "$dst" ]] || ! diff -qr "$src" "$dst" >/dev/null; then
      source_error "Module mirror $name differs. Review both copies and synchronize deliberately; refusing to overwrite."
      return 1
    fi
  else
    mkdir -p "$AC_DIR/modules" || return 1
    cp -a "$src" "$dst" || return 1
  fi
}
