# Selected roster characters as ordinary world bots

Activated September 19 after the explicitly authorized worldserver-only deployment.
All seven selected bots were confirmed online. See
[deployment verification](world-bots-deployment-20260919.md) for backups, preservation
checks and remaining live-behavior limits. Further restarts require fresh permission.

**Follow-up fix deployed September 19 at 17:36:30 UTC:** saved raid profiles were
found to overwrite solo questing/grinding defaults. The correction is in playerbots
fork `f63a04634971d666744ea68f7d1a281c1b0eb715`; see
[the correction and its tests](world-bots-solo-strategy-fix.md) and
[initial quest/XP observations](world-bots-solo-deployment-20260919.md).
Being online alone was not proof of autonomous leveling.

**September20 follow-up:** [solo-only BG admission, friend cap15 and the `stats`
filter are deployed](solo-companion-deployment-20260920.md).

**Deployed September20 at 14:42:16 UTC:**
[solo healer-DPS restoration and druid travel-form exits](solo-healer-dps-and-druid-forms.md),
plus the [human-led group BG queue repair](player-led-battleground-queues.md).
The latter corrects the first solo safeguard's cancellation of deliberate player
group queues without restoring autonomous grouped queueing. See
[deployment verification and the offline LFG group/save exception](player-led-bg-deployment-20260920.md).
Live BG-popup and repeated underwater-combat acceptance remain outstanding.

## Active settings

The deployed incremental patch `0038-playerbot-roster-world-bots.patch` uses this
startup-only playerbots option:

```ini
AiPlayerbot.WorldBotGuids = "1118,1159,1180,1274,1297,1315,1433"
```

These are Raney, Beliona, Ailina, Pilbok, Kaaren, Keilmere and Feelesia respectively.
The option defaults empty. It accepts existing characters on type-2 **random bot**
accounts only; invalid entries are logged/ignored and duplicates collapse. It does
not select whole accounts or read/change saved roster membership. Arinerica (142),
Meliah (815), and the other 33 saved roster characters are not opted in.

**Required alongside enrollment:** the existing level-bracket exclusions now include
all seven names, retaining Arinerica and Meliah. Preserve other existing exclusions
when adapting this setup. The active nine-name list is:

```ini
BotLevelBrackets.ExcludeNames = "Meliah,Arinerica,Raney,Beliona,Ailina,Pilbok,Kaaren,Keilmere,Feelesia"
```

The existing setup source is `BOT_LEVEL_BRACKET_EXCLUDE_NAMES` in `.env`. Both env
copies and the mounted bracket config were updated during the authorized deployment.
The bracket module directly calls factory randomization independently of the world
scheduler; **enrollment alone cannot guarantee quest-earned levels** without this
exclusion. Neither bracket source nor setup/env wiring was changed.

Requires the existing Enabled/RandomBotAutologin switches. Existing
DisabledWithoutRealPlayer=0 permits operation with the human offline; a value of 1
still enforces its normal delay/logout policy. Ordinary periodic-online/offline,
death-knight-login, fixed-level and AI activity settings continue to apply.
The source default is empty; the local deployment explicitly enables these seven GUIDs.

## Lifetime versus behavior

- Each unique valid GUID adds one slot beyond the normal population target (seven
  above 2000 here). Ordinary bots are not evicted or replaced, including at startup
  or when the normal pool is already full. Enrollment uses existing add/update/
  logout events, retry and periodic lifetime policy, not a new duty subsystem.
- The global holder owns their sessions. Existing `IsRandomBot` already accepts
  random-account characters enrolled in `currentBots`, including type 2. Existing
  AI factory RPG/grind strategies, quest travel, group/master transitions and human
  logout detachment are reused; no new autonomous progression was implemented.
- Offline `.playerbot bot login NAME` requests retain original authorization and
  bot-count checks, then route selected GUIDs globally. **Login no longer itself
  auto-invites selected characters.** Online login still says already logged in.
  Use a normal `/invite NAME` for an online world bot, as with Ari/Meliah; existing
  invitation security, group follow and configured summon behavior remain intact.
  Leaving the group releases the master; human logout does not log out world-held
  bots. Grouping/security restrictions can still prevent invitations.
- Raidroster's personal-manager login/trimming commands are unchanged and are not
  adapted to manage world-held sessions. Roster rows/owners remain intact; do not
  treat its personal-manager counts or login commands as a new world-bot interface.
- Startup-only: reloading config does not add/remove this selection or hand off
  ownership. A bot already connected under another holder or loading is not stolen
  or duplicated. Removal from the option takes effect at the next authorized
  restart, returning it to ordinary addclass login. Existing startup initialization
  already clears add events; no new account/event cleanup or GetBots filtering was
  added. GetBots retains its original handling of every pre-existing add event,
  including unselected type-2 events. No roster/account/save reset is part of removal.

## Progression and maintenance boundaries

Selected characters skip automatic scheduler factory randomization and automatic
addclass login level matching. This prevents that lifecycle's random level rolls,
quest resets and whole-character replacements; it is **not an equipment freeze**.
Explicit administrative init/random/reset commands are unchanged and remain capable
of resetting characters. Do not use them to validate this feature.

Normal auto-upgrades, learning, talent picking, vendors and ordinary refresh remain.
Refresh (including revival maintenance) can replace bag contents/provision supplies
and apply existing attunement, reputation, skills and talent behavior. This is not
strictly loot-only gearing or an entirely organic-only lifecycle. The original
enrollment change did not alter friend protection or its effect on level-up equipment
initialization; its current cap is15 as described below. No special inventory-preservation,
security or global-cheat rules were added.

## Friend-protection cap — 15 active

The [authorized September20 deployment](solo-companion-deployment-20260920.md) raised
live `AiPlayerbot.PersistentCompanionMaxPerAccount` from5 to15. Both private `.env`
copies and the public example use `PERSISTENT_COMPANION_MAX_PER_ACCOUNT=15`.
An omitted env value retains the native default5; zero still means unlimited.

The originally staged cap9 was insufficient: preflight found eleven eligible bot
friends, including Alenaron and Emia. GUID ordering would exclude Feelesia at cap9.
The user explicitly selected15, which covers all eleven and leaves four slots of
headroom. Offline bot friends still count; the limit is per human account and ordered
by eligible bot GUID, not friend-list display order or membership in WorldBotGuids.

The [deployed solo BG policy](solo-companion-battlegrounds.md) replaces the blanket
BG exclusion: friends and selected world bots may queue while free/solo, but not in
a party or raid. Other existing protections (scheduler teleports, LFG filler and
maintenance effects) remain. This is not an equipment freeze.

A plain restart does not apply an env-only value: the runtime config must also be
updated during an authorized change. Do not run full setup casually on the live
server. No C++ rebuild is needed for a cap-only change.

Offline test: `python3 -m unittest scripts.tests.test_companion_cap_setup -v`.
It exercises explicit caps9/15, default5, unlimited0, idempotence and preservation of
other settings through the actual setup setter against temporary files only.
The [bare `stats` chatter filter](chatter-command-filter.md) is also active.

## Offline evidence and reconstruction

Run `python -m unittest scripts.tests.test_roster_world_bots -v`. It compiles actual
source-extracted selection/enrollment/classification/idle-maintenance/group-master
methods with small API doubles under C++20 warnings-as-errors and UBSan, plus checks
wiring in the real scheduler/login/startup code. Tests cover malformed/default/
disabled input, individual/account negatives, duplicates, connected/pending ownership,
ordinary population already full, periodic lifetime, routing, deferred randomization,
unchanged refresh/revive/friends and grouped/ungrouped/battlefield transitions.
An executable case preserves GetBots restoration of unselected type-2 add events
with the option empty. Source-order checks verify that the core loads the character
cache before OnBeforeWorldInitialized, whose playerbots hook creates/loads random
accounts, assigns account-type lists, then calls Init/LoadWorldBotGuids.
These are not linked server or real quest progression tests; async callbacks,
full scheduler execution, DB event expiry and invitation success remain unverified live.

The original0038 source acceptance included three focused tests, an incremental patch
roundtrip, production-header syntax checks and independent review. That coverage did
not exercise the saved-strategy replacement responsible for the later live stall.
The suite now also compiles the real repository-load and post-load repair methods;
see the correction document for the added cases and limits.

Source recovery now uses our committed forks and `repo-pins.txt`, not patch replay.
Patch0038 remains historical reference. Its older temporary source/review backup was
removed during the explicitly requested cleanup; the latest deployment backup remains.
Do not reset native trees or try to reconstruct the current source by replaying old patches.
