# Saved-profile solo strategy fix — deployed September 19, 2026

Playerbots fork commit:
[`f63a04634971d666744ea68f7d1a281c1b0eb715`](https://github.com/CmPons/mod-playerbots/commit/f63a04634971d666744ea68f7d1a281c1b0eb715).
Root `repo-pins.txt` records this published revision. No historical patch replay is needed.
The authorized worldserver-only deployment became ready at **17:36:30 UTC**;
see [deployment evidence and initial activity](world-bots-solo-deployment-20260919.md).

This addresses the [observed solo stall](world-bots-organic-leveling-investigation.md):
login creates free-world-bot defaults, then a saved raid profile clears/replaces them.
The bots remained online, but their non-combat strategy sets lacked `grind` and `new rpg`.

## Narrow correction

`PlayerbotHolder::OnBotLogin` now calls
`RandomPlayerbotMgr::RestoreWorldBotSoloStrategies` immediately after saved settings load.

- Only GUIDs selected by `AiPlayerbot.WorldBotGuids` are affected; real players, Ari/Meliah
  and all other bots retain their existing behavior.
- Reconciles only four non-combat activity strategies: `grind`, `new rpg`, `rpg`,
  `move random`. Combat/spec/CC preferences, other strategies, dead behavior and saved
  context values are not wiped. No repository reset/save is introduced.
- A free overworld bot gets `grind` plus the existing configured factory choice:
  `new rpg`, or legacy `rpg` if questing is enabled, otherwise `move random`.
- Grouped or mastered bots, instances, battlegrounds/queues and active Wintergrasp
  combatants do not get these solo activities from a saved profile.
- Explicit stay/passive/runaway/move-from-group strategies in either active state,
  and combat follow/leash, suppress restoration. Ordinary non-combat `follow` is a
  factory default even without a master; it alone does not suppress solo activity.
- This is a login correction, not a per-tick forced setting. Subsequent manual orders
  are not repeatedly overridden. Existing group/master transitions reset to normal
  party or solo factory behavior as before.

No XP/level/gear grants, synchronization, quest completion, roster/account conversion,
teleport command, new companion framework or global activity/config changes. Ordinary
world-bot maintenance and its existing boundaries are unchanged.

## Validation

`python3 -m unittest scripts.tests.test_roster_world_bots -v`: **4 tests passed**.
The new C++ fixture executes source-extracted production repository loading, restoration,
solo factory choices and group/master transition code with API doubles, C++20/Werror
and UBSan. It first reproduces the missing-strategy state from Ailina's saved profile,
then exercises the actual login load/repair call sequence.

Coverage includes all seven GUIDs; null/missing AI; humans/unselected bots; absent/saved
profiles; idempotence; preserved combat/CC/context values; manual controls; all three
configuration modes; saved solo profiles loaded into groups/mastered/PvP/instance states;
and normal join/leave behavior without continual reassertion against later manual orders.
Database doubles provide reads only: a new write in the tested methods would fail to compile.
Group-transition factory scaffolding is an API double, not a linked live server test.

Both modified `.cpp` files passed `scripts/tests/raid_combat_syntax.py` against real
production headers. Native whitespace checks passed; focused codestyle findings matched
the pre-existing baseline exactly, with no new findings. The subsequent authorized image build/link and runtime dependency checks
also passed; initial live observations are recorded in the deployment document.

## Activation and acceptance

**This fix is active.** The user separately authorized building and restarting after
source acceptance. Only the worldserver was recreated; future interruptions require
fresh permission. Initial observations include new quests, two turn-ins by Pilbok,
and XP gains among previously stalled bots, with the human offline and bots solo.

For continuing acceptance, verify actual non-combat strategies and observe an ungrouped bot
accepting/progressing/turning in ordinary quests or earning sustained solo combat XP.
Being online, receiving scheduler teleports or earning XP in the human's party is not
sufficient acceptance. Also verify invitation/follow behavior and explicit stay/passive
orders. No raidroster level sync or factory reroll is part of that verification.
