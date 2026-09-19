# Solo world-bot leveling investigation — September 19, 2026

**Follow-up:** [the source correction](world-bots-solo-strategy-fix.md) is committed
and pushed as `f63a04634971d666744ea68f7d1a281c1b0eb715`; it is not deployed yet.
The observations below describe the pre-fix running server.

## Finding

Enrollment/logins work, but saved raid strategy profiles override the normal solo
world-bot strategies at login. The five bots that were already ungrouped lack both
`grind` and `new rpg` in their saved non-combat profiles. This is a missing integration
step in the world-bot activation, not a need to synchronize their levels.

No gameplay/source/config changes, bot commands, DB writes, builds or restarts were
performed during this investigation. Queries were read-only. Private observations are
under the retained deployment backup's `organic-leveling-investigation/` directory.

## Observations

Before the user's summon, Raney and Beliona were grouped with Redshift, Arinerica and
Meliah. Their earlier XP gain therefore did **not** demonstrate autonomous leveling.
The other five were online and ungrouped:

| Bot | Level | Saved XP | Saved location before summon |
| --- | --- | --- | --- |
| Ailina | 60 | 0 | Stormwind |
| Pilbok | 60 | 1,680 | Burning Steppes |
| Kaaren | 60 | 0 | Winterspring |
| Keilmere | 60 | 0 | Hellfire Peninsula |
| Feelesia | 60 | 0 | Zangarmarsh |

All five had only an old AQ ring quest, and zero rewarded-quest rows. Pilbok's small
XP gain alone does not establish questing or its XP source. Scheduler records show
normal updates and relocation/refresh events for the four not friend-protected.

The user then summoned them to Honor Hold to observe them, without grouping them.
The subsequent query confirmed **all nine bots and Redshift had no group**. The five
above shared the summon coordinates and the same XP values. The user directly observed
them standing idle. Character positions/XP are saved snapshots (configured five-minute
save interval), not continuous live AI telemetry.

At the later sample, saved profiles for the four previously grouped bots were absent;
the five old raid profiles remained. Group-reset handling can clear saved profiles and
restore defaults, but no live command/event trace was captured to prove the exact cause
of those four removals. Their behavior must be assessed separately.

## Source trace

References below use the published fork revision whose gameplay source matches the
running build:

1. [AiFactory.cpp:608–647](https://github.com/CmPons/mod-playerbots/blob/b5bbd22d20a374acf232e321477bf31429a4585b/src/Bot/Factory/AiFactory.cpp#L608-L647)
   gives free random bots `grind` and, with the active setting, `new rpg`.
2. [PlayerbotMgr.cpp:536–545](https://github.com/CmPons/mod-playerbots/blob/b5bbd22d20a374acf232e321477bf31429a4585b/src/Bot/PlayerbotMgr.cpp#L536-L545)
   resets strategies, then loads saved playerbot settings.
3. [PlayerbotRepository.cpp:27–40](https://github.com/CmPons/mod-playerbots/blob/b5bbd22d20a374acf232e321477bf31429a4585b/src/Db/PlayerbotRepository.cpp#L27-L40)
   **clears** the default combat/non-combat strategy sets before applying saved sets.
   All five saved non-combat sets include `aq40` and `quest`, but neither `grind` nor
   `new rpg`. They do not contain `stay` or `passive` either.
4. [QuestStrategies.cpp](https://github.com/CmPons/mod-playerbots/blob/b5bbd22d20a374acf232e321477bf31429a4585b/src/Ai/Base/Strategy/QuestStrategies.cpp#L11-L33)
   shows that plain `quest` handles shared quests/gossip/completion interactions; it is
   not autonomous quest selection/travel.
5. [NewRpgStrategy.cpp](https://github.com/CmPons/mod-playerbots/blob/b5bbd22d20a374acf232e321477bf31429a4585b/src/Ai/World/Rpg/Strategy/NewRpgStrategy.cpp)
   provides the autonomous activity/quest loop;
   [GrindingStrategy.cpp](https://github.com/CmPons/mod-playerbots/blob/b5bbd22d20a374acf232e321477bf31429a4585b/src/Ai/Base/Strategy/GrindingStrategy.cpp#L19-L29)
   supplies proactive `attack anything` targeting.
6. [RandomPlayerbotMgr.cpp:1753–1774](https://github.com/CmPons/mod-playerbots/blob/b5bbd22d20a374acf232e321477bf31429a4585b/src/Bot/RandomPlayerbotMgr.cpp#L1753-L1774)
   can refresh/teleport idle bots independently of that quest loop. Relocation is not
   proof of productive adventuring. Living-bot `Refresh` does not rebuild strategies;
   [its reset only clears transient AI state](https://github.com/CmPons/mod-playerbots/blob/b5bbd22d20a374acf232e321477bf31429a4585b/src/Bot/PlayerbotAI.cpp#L863-L925).

This explains the observed stall, but active in-memory strategy lists were not queried
through chat or a debugger. No claim of comprehensive live-AI instrumentation is made.

## Other gates checked

- World GUIDs are enabled; level cap70; fixed random level0; bots allowed without a
  real player. The problem is not a configured level60 cap.
- All belong to Redshift's human-led guild. The active guild-force setting bypasses
  ordinary solo activity throttling through the
  [guild activity check](https://github.com/CmPons/mod-playerbots/blob/b5bbd22d20a374acf232e321477bf31429a4585b/src/Bot/PlayerbotAI.cpp#L4699-L4705).
- Existing friend protection covers only the first five eligible GUIDs per account,
  [ordered by GUID](https://github.com/CmPons/mod-playerbots/blob/b5bbd22d20a374acf232e321477bf31429a4585b/src/Bot/RandomPlayerbotMgr.cpp#L737-L814).
  Ailina is the fifth; her scheduler teleport is suppressed. The other four can be
  teleported. This explains the different initial locations, not the missing quest AI.
- Automatic whole-character rerolls are skipped for the selected world bots; ordinary
  refresh/provisioning remains unchanged. This is not a strictly loot-only maintenance
  system, and the investigation did not change it.

## Recommended correction — not implemented here

Restore the existing solo `grind`/`new rpg` behavior for opted-in world bots **after
saved settings load**, only when genuinely free. Cover group/master transitions so
recruitment returns them to ordinary party behavior. Preserve combat/spec preferences
and explicit manual controls; do not indiscriminately wipe saved settings, reroll gear,
grant XP or levels, change the whole bot population, or introduce a new companion system.

Regression coverage must include a real saved raid profile replacing factory defaults,
not just enrollment and default strategy creation. Live acceptance should demonstrate
new quests/objectives/rewards or sustained XP gains while ungrouped, not merely online
status, teleportation or XP earned with the human. A native deployment would need a
separately authorized restart; none is authorized by this investigation.
