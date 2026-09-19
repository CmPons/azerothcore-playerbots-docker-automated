# Source ownership and publication

## Where our work lives

| Local repository | GitHub fork | Branch |
| --- | --- | --- |
| Workspace root | [CmPons/azerothcore-playerbots-docker-automated](https://github.com/CmPons/azerothcore-playerbots-docker-automated) | `main` |
| `azerothcore-wotlk` | [CmPons/azerothcore-wotlk](https://github.com/CmPons/azerothcore-wotlk) | `local-playerbot` |
| `azerothcore-wotlk/modules/mod-playerbots` | [CmPons/mod-playerbots](https://github.com/CmPons/mod-playerbots) | `local-playerbot` |
| `azerothcore-wotlk/modules/mod-player-bot-level-brackets` | [CmPons/mod-player-bot-level-brackets](https://github.com/CmPons/mod-player-bot-level-brackets) | `local-playerbot` |

These are separate repositories, not submodules. The root ignores the native checkout;
root `git status` alone cannot show unsaved native work. Each maintained checkout has
remote `fork` pointing to our repository. Existing `origin` remotes remain upstream for
reference; fresh setup checkouts use their declared source URL as `origin` too.

Root `modules/` contains our authored chatter, raid-roster, raid-scaling, AH-price,
Wintergrasp and arena-roster modules. Commit these in the root repository and keep
`azerothcore-wotlk/modules/` copies synchronized. Other external modules are pinned
upstream dependencies; fork them before making local modifications.

## Required finish procedure

1. Inspect diffs, tests and untracked files in **every affected repository**.
2. Commit and push native changes to `fork/local-playerbot` (no force-push).
3. Update the corresponding exact SHA in root `repo-pins.txt`.
4. Commit and push root changes to `fork/main`.
5. Run `./scripts/repo-status.sh --remote`. Report any dirty tree, pin mismatch,
   unavailable remote or unpushed commit; do not call local-only work saved.

`AGENTS.md` in each maintained repository requires this. Checkpoint unfinished work
before pausing. Do not conceal changes with resets, cleaning, stashing or ignore flags.
Do not commit credentials, `.env`, live configs, dumps or compiled output. Configuration
that needs reproduction belongs in non-secret templates/scripts; private state belongs
in backups. Daily database/env backups are not a replacement for source publication.

## Setup and update safety

`repo-pins.txt` is a required manifest of exact published source revisions. Our fork
commits already contain the local native modifications. `patches/` is retained as
historical documentation, **not applied by setup/update**. No pristine-upstream patch
replay is needed to recover the current native source.

```bash
./scripts/repo-status.sh           # local status and pin checks
./scripts/repo-status.sh --remote  # also verify our fork branch tips against local HEAD
./setup.sh --sources-only          # clone/fast-forward pinned sources only
./update.sh --sources-only         # same, no build
./update.sh                        # then build worldserver image; DO NOT deploy/restart
```

Source preparation refuses dirty checkouts (including untracked source), missing pins,
nonempty non-repository directories, and ahead/divergent history. Existing sources can
only fast-forward; nothing is reset or cleaned. A newer local commit requires publication
and a new pin, not a rewind. If shallow history prevents ancestry verification, fetch the
needed history explicitly rather than bypassing the refusal.

Differing root-module/build-tree mirrors require inspection and deliberate synchronization;
they are never automatically overwritten. Unknown module directories require explicit review,
not deletion. A missing mirror is copied from the root source.

Full `setup.sh` still prepares runtime configuration and starts services: do not run it
on a playing server without authorization. `update.sh` now builds only worldserver; it
never imports databases, changes volumes, or starts/restarts containers. Deployment remains
a separate authorized operation with fresh backups and preservation checks.

For upstream upgrades, merge/test deliberately on our branches, publish, and advance
pins. For rollback, use an explicitly reviewed revert or a separate checkout at the older
revision; automatic update never rewinds a branch. Git recovery does not undo DB migrations.

## September 19 publication checkpoint

The previously local native commits and working-tree source were published without
rewriting gameplay code:

- Core: `b6e03792268af131467f46f2e7455dc5e1e82c5d` (includes the three previously unpushed AQ commits).
- Playerbots: `b5bbd22d20a374acf232e321477bf31429a4585b` (includes prior local commits,
  all formerly untracked AQ/raid-policy/SWP code, and vendored Lua sources/provenance).
- Level brackets: `a2614288ebe41093b8915b3ee3002bcfc8a63c1a` (includes the previously local friend-GUID fix).

The existing root `flake.lock` is preserved byte-for-byte and versioned; no Nix inputs
or bridge runtime were changed. Vendored Lua whitespace was retained rather than altering
third-party code during the preservation checkpoint. No server build/restart, DB operation,
config change or upstream upgrade was part of this publication task.
