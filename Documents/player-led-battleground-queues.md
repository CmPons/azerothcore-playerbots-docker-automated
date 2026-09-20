# Player-led companion BG queues — source fix, not deployed

September 20, 2026. The user reported that **Join as Group**, which had worked
before, now produced no enduring minimap icon or invitation and no error. The
client reported `none` for both BG slots. Separately, the missing dungeon-finder
icon was confirmed to be Atlas covering it; that UI issue is unrelated.

The user authorized this source repair and clarified that it must apply when
bots are grouped with **any real player**, not just Redshift. No build, live
configuration change, reload, restart or deployment is authorized by this work.

## Regression in the deployed policy

The September 20 playerbots revision `a72a2ac66c2d438daaef5e48fd50751eb63cd81f`
used `CanAutoJoinBattleground` for both autonomous queue submission and invitation
acceptance. A protected companion in any group fails that policy, including a
party explicitly queued by its human leader. Its WAIT_QUEUE/WAIT_JOIN handler
then sent a leave-queue packet.

The core's `WorldSession::HandleBattleFieldPortOpcode` leave branch iterates
**all members of that queue group**, removing them and sending STATUS_NONE.
Thus a bot could cancel the human's queue without a join-error message. The
same old guard at final port consumption would also veto that bot's entry.
This code path explains the observation; no live packet trace was captured.

## Corrected separation of intent

- Autonomous queue submission still uses the unchanged solo-only policy.
  Party/raid leaders and members, pending invitations, masters and manual orders
  continue to block autonomous companion queueing. Solo BG leveling remains enabled.
- Core `GroupQueueInfo` now retains the original queue group's GUID and the queue
  initiator's GUID. Solo entries have an empty group GUID. These fields are
  initialized in `BattlegroundQueue::AddGroup`, are in-memory only, and survive
  members accepting invites and leaving the queue's remaining `Players` set.
- New `CanAcceptBattlegroundQueue` permits a protected companion's ordinary BG
  queue when it came from the **same current group**, initiated by a connected
  human who remains a member. The human must still be in that queue or have
  entered that queue's invited BG. There is no player-name/account/GUID allowlist.
- The original party remains valid if the human enters first and moves into the
  BG auto-raid: `GetOriginalGroup()` and the invited instance identify that case.
  Merely finding any human in an old solo-queued bot's new party is not consent.
- WAIT_QUEUE, WAIT_JOIN, force-entry recovery, the legacy Warsong accept action
  and the final port hook use the acceptance policy. The final hook rechecks the
  exact queue after any intervening party changes. Explicit client leave requests
  remain allowed. Existing BG auto-raids are not forcibly recalled.
- A deliberate human group queue may proceed despite disabled **autonomous** BG
  joining or a saved follow/stay strategy. It does not respec the bot, reset saved
  preferences, or grant new arena participation. Normal core eligibility still
  applies: level bracket, deserter, LFG conflicts, group size, combat/port checks.
- Ordinary filler bots and human-controlled characters retain their existing rules.

The exception requires the original human initiator to remain connected and in the
original party (including as their original group after entering the BG). Losing
that association revokes the protected companions' exception; simply changing
which member is party leader does not. Recreating a party does not reuse consent.

## Cancellation and threading

For ordinary BGs, policy rejection now removes **only the ineligible bot**, rather
than synthesizing a client packet which would cancel the whole queued group.
The bot's own queue-slot status is cleared and matchmaking is rescheduled. Other
members' queues, group membership and AI strategies are not reset by this cleanup.
Core rated-arena removal semantics remain unchanged; this repair does not authorize
companion arena queues.

Cleanup is dispatched through the existing `PlayerbotWorldThreadProcessor`, not
performed from an AI/map-thread action. The operation holds only the bot GUID and
queue type, resolves the player again, and rechecks acceptance on execution. A
stale cleanup therefore cannot clear a newly authorized player-led queue or recall
an already-entered bot. Disconnected/missing players and missing queue records are
safe no-ops. The normal status/port safeguards remain if an operation is deferred.

## Validation

```sh
python -m unittest scripts.tests.test_solo_battlegrounds \
  scripts.tests.test_roster_world_bots scripts.tests.test_druid_healer_forms \
  scripts.tests.test_chatter_commands scripts.tests.test_companion_cap_setup -v
```

- Twelve focused tests passed. C++20/Werror/UBSan fixtures extract production
  policies, packet hooks, status guard, cancellation operation, queue-info struct
  and the actual queue-provenance assignments. Queue storage, packet delivery,
  player/group objects and world-thread scheduling are API doubles.
- The deployed policy was tested with the new human-group scenario and failed
  `Assertion '!StatusGuard(&bot, STATUS_WAIT_QUEUE)'`, reproducing the regression.
  The legacy run uses actual old policy/hooks/status code, with the new core
  provenance fixture, not a rebuilt production server.
- Coverage includes all nine companions, two different human GUIDs, parties/raids,
  human-first entry, already-entered bots, bot-led queues, missing/different queues,
  old solo invites, different/recreated groups, absent/departed humans, pending
  invites, arenas, final-port races, bot-only cancellation, new consent before
  deferred cleanup, entry/logout before cleanup, and the prior solo-policy matrix.
- Five changed translation units passed production-header `-fsyntax-only`: core
  `BattlegroundQueue.cpp` and the four playerbots CPP files. The initial syntax
  pass caught a bracket-enum conversion; the corrected final pass succeeded.
- Scoped native codestyle findings matched the existing baseline; added lines
  passed width/whitespace checks and all affected diffs passed `git diff --check`.

Private evidence: `backups/player-led-bg-fix-20260920-154449/`, with its local pointer
at `/tmp/player-led-bg-fix-backup`. No image build/link, live group-queue exercise,
actual BG-popup observation after the fix, or end-to-end concurrent race test has
been performed. These remain deployment acceptance work, not claims from fixtures.

## Publication and future deployment

The maintained core and playerbots forks must be deployed together: the playerbots
acceptance helper consumes the new core queue provenance. Published source:

- Core: `897c2c6d72632ad6e3f1c01a9e82815ce2c165a4`.
- Playerbots: `693840886d1db462c141f4d31cbb606eb96b8a84`.

These revisions are pinned in root `repo-pins.txt`. No SQL migration or runtime setting
change is required. The earlier healer-DPS/druid-form source repair remains in the
pinned playerbots history and would also be included in a build of these sources.

The running server remains the September 20 solo-BG image
`sha256:1c43fb094c0f397f145c1055d63986f5f4993931ae7ab6ba7c95373c38465601`;
this repair is **not active** until a separately authorized build/deployment.
Before deployment, take fresh backups and verify source/config/state preservation.
After deployment, verify human-led group queue status/popup, both human-first and
bot-first entry, and solo-queue recruitment without disturbing the new party.
