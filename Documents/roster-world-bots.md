# Selected roster characters as ordinary world bots (source only)

Source implementation accepted after independent review and parent validation; not
activated or deployed. No service interruptions, live bot commands, DB writes or runtime
config edits were performed. Do not restart while the user is playing.
Build/deployment/configuration/restart require a separate authorized step.

## Required activation settings (do not apply now)

After deploying incremental patch `0038-playerbot-roster-world-bots.patch`, the
startup-only playerbots option would be:

```ini
AiPlayerbot.WorldBotGuids = "1118,1159,1180,1274,1297,1315,1433"
```

These are Raney, Beliona, Ailina, Pilbok, Kaaren, Keilmere and Feelesia respectively.
The option defaults empty. It accepts existing characters on type-2 **random bot**
accounts only; invalid entries are logged/ignored and duplicates collapse. It does
not select whole accounts or read/change saved roster membership. Arinerica (142),
Meliah (815), and the other 33 saved roster characters are not opted in.

**Required before activation:** extend the existing level-bracket exclusions to
include all seven names, retaining Arinerica, Meliah and any other existing entries.
The known nine-name list is:

```ini
BotLevelBrackets.ExcludeNames = "Meliah,Arinerica,Raney,Beliona,Ailina,Pilbok,Kaaren,Keilmere,Feelesia"
```

The existing setup source is `BOT_LEVEL_BRACKET_EXCLUDE_NAMES` in `.env`. Update both
through the established deployment/setup process later, not by this source patch.
The bracket module directly calls factory randomization independently of the world
scheduler; **enrollment alone cannot guarantee quest-earned levels** without this
exclusion. Neither bracket source nor setup/env wiring was changed.

Requires the existing Enabled/RandomBotAutologin switches. Existing
DisabledWithoutRealPlayer=0 permits operation with the human offline; a value of 1
still enforces its normal delay/logout policy. Ordinary periodic-online/offline,
death-knight-login, fixed-level and AI activity settings continue to apply.
No option is turned on by this change.

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
strictly loot-only gearing or an entirely organic-only lifecycle. Existing friends
protection and its max-five cap are unchanged, including its existing effect on
level-up equipment initialization. No special inventory-preservation, security or
global-cheat rules were added.

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

The parent independently reran all three focused tests and the exact patch roundtrip;
independent review found no issues. Both changed native translation units also pass offline
`-fsyntax-only` against real production headers using `scripts/tests/raid_combat_syntax.py`.
Focused C++ style checks retain the same 11 pre-existing alignment findings, with no new
findings. No CMake or native/image build was run. Patch 0038 is an incremental diff of the **actual pre-task layered bytes**
from `backups/roster-world-bots-20260919-161532/source-before.tar.gz`, not nested HEAD.
Apply after existing layers from the core tree (`git apply`), never replay/reset them.
Private `actual-task.diff` and `validation.txt` in that backup directory record full
review scope, patch roundtrip/byte equality, commands, and retained baseline state.
