# Player-led BG queues and healer fixes — deployed September 20, 2026

The user explicitly authorized **build and deployment** after publication of the
source repair. Only the worldserver was stopped/recreated. Ready:
**2026-09-20T14:42:16.466133413Z** (16:42:16 local). Further interruptions need fresh
permission. No live setting was changed.

## Active changes

- [Player-led group BG queues](player-led-battleground-queues.md): explicit queues
  led by any real player are distinguished from a bot's autonomous solo queue;
  human-first BG entry is supported. Autonomous grouped companion queue submission
  remains blocked. Policy cleanup removes only an ineligible bot from an ordinary
  BG queue, on the world thread, without clearing the human's queue.
- [Solo healer-DPS repair and druid form exits](solo-healer-dps-and-druid-forms.md):
  eligible selected solo healers receive additive offensive support after saved
  profile loading; healer damage can cancel aquatic/travel form without disabling
  normal noncombat swimming or changing healing priorities.

## Revisions and runtime identity

- Root build-source checkpoint: `4e7f31795a054fd880948356ee8d314dd34b969b`.
- Core: `897c2c6d72632ad6e3f1c01a9e82815ce2c165a4`.
- Playerbots: `693840886d1db462c141f4d31cbb606eb96b8a84`, including the earlier
  healer/form revision `c806c14b0cc35e29c42402ef9f13d0ba3796a29a`.
- Image: `sha256:e77033aec686e3671691f037b42e311e2aa914f1c7cd22e8834b8fac6cc1f0e7`.
- Image tag: `acore/ac-wotlk-worldserver:player-led-bg-20260920-162702`, also `:master`.
- Running binary SHA256: `1f3ad74ab4d760f3198922fc6fe9ca1181bfa3468f6ae35bc44dd7b4342a8525`.
- Container started: `2026-09-20T14:41:59.315027167Z`; readiness at 14:42:16 UTC;
  running, no OOM, restart count0 at final verification.
- Rollback image retained as
  `acore/ac-wotlk-worldserver:pre-player-led-bg-20260920-162702`:
  `sha256:1c43fb094c0f397f145c1055d63986f5f4993931ae7ab6ba7c95373c38465601`.

## Build and restart controls

- Twelve focused tests rerun before building; source-stage production-header syntax,
  C++20/Werror/UBSan regression and baseline-matched codestyle evidence retained.
- Native image build 16:27:58–16:41:07 local; successful build/link. Candidate tag
  did not replace `:master` or touch the running container during the build.
- Image inspected using a never-started, networkless container. Binary checks found
  acceptance/cancellation methods, the deferred cleanup operation, final BG hooks,
  and the druid healer strategy. Disassembly verified repository loading precedes
  solo-strategy repair. Networkless `ldd` found no missing runtime libraries.
- Source delta from the previous image was exactly **eight native files**: two core
  queue files and six playerbots source/header files across the two repairs.
  All5,062 source/Git records remained unchanged through build/start verification.
- All48 configuration/policy records and authored-module mirrors remained exact.
  Level70 caps, friend cap15, `stats` filtering and other gameplay settings retained.
- Core updater disabled by `AC_UPDATES_ENABLE_DATABASES=0` (the unchanged mounted
  config's value7 is overridden). Enabled playerbots updater matched its actual28
  applied SQL files before/after; all core/PB update ledgers unchanged.
- No humans online immediately before shutdown. Worldserver stopped cleanly,
  exit0/no OOM, and a fresh four-database stopped backup was verified before start.
- Recreated using only:
  `docker compose up -d --no-deps --no-build --pull never --force-recreate ac-worldserver`.
  No setup, DB importer, client-data helper or dependency start was performed.
- Authserver, database, importer/client-data helper identities/start times and Pi
  bridge PID/invocation were unchanged. Bridge `/api/tags` health succeeded without
  generating a model request.

## State preservation and reviewed exceptions

Exact stopped/immediate-start matches for the tracked ten characters:
**items/inventory, level/XP, quests, roster and saved profiles**. The full saved
roster table was compared. All nine managed bots were online after scheduling
settled. Existing saved Ailina/Keilmere profiles were not rewritten by deployment.

**Group/save exception — reported to the user:** the existing `KeepAltsInGroup=0`
login cleanup disbanded offline Dungeon Finder party1374. Its completed normal
Steamvault instance749 (map545, difficulty0, encounter mask7, `S V 3 3 3`) and
five **non-permanent** binds were also cleared. These were the entire save delta;
there were no raid saves or permanent binds in the stopped snapshot.

This was not a data migration or an operator reset. Existing
`PlayerbotHolder::OnBotLogin` leaves/disbands a group lacking a valid online master
under that configuration. `Group::Disband` resets normal instances for members,
including the offline leader; `InstanceSaveMgr::DeleteInstanceSaveIfNeeded` removes
an unbound, unloaded instance. The stopped dump shows the normal instance's expiry
was still in the future, so this must not be described as an expired timer.
No group command, gameplay SQL write, backup restoration or config override was used.
The old party needs to be reformed.

Post-start diagnostics also included vendor-data warnings, an invalid teleport
request for non-roster Carius2029, and a duplicate `pet_spell` insert for pet69321,
spell7802, owned by non-roster Keelthen2340. These were recorded, not repaired or
claimed to have been proven present before deployment. Existing priority-permission
and missing chatter GroupJoin-option diagnostics also remain. No fatal/OOM/restart
was observed.

## Backups, evidence and acceptance limits

Private directory: `backups/player-led-bg-deploy-20260920-162702/`.
Local pointer: `/tmp/player-led-bg-deploy-backup`.

It contains fresh before-build and stopped four-database dumps, config/policy archive,
source/config manifests, exact delta checks, scoped snapshots/diffs, updater evidence,
image/binary/disassembly/linkage checks, runtime identities, logs, and
`deployment-closeout.json`. `STOPPED-SHA256SUMS` and `DEPLOYMENT-SHA256SUMS` verified.
The fresh stopped dump is the recovery authority; never overwrite newer gameplay
with an older deployment backup. Scripts there have already run; do not rerun blindly.

Deployment/binary identity is verified, but **a human-led BG queue popup/entry has
not yet been observed after this deployment**. Repeated underwater combat and
post-kill swimming also still need live acceptance. Source fixtures are not proof
of a real concurrent queue/recruit race or of damage actually landing in water.
