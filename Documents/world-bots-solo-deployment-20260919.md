# Solo strategy repair deployment — September 19, 2026

The user explicitly authorized building and restarting after source acceptance.
Only the worldserver was stopped/recreated. Ready: **2026-09-19 17:36:30.104088897 UTC**
(19:36:30 local). Subsequent interruptions require fresh permission.

## Exact deployment

- Playerbots: `f63a04634971d666744ea68f7d1a281c1b0eb715`.
- Core: `b6e03792268af131467f46f2e7455dc5e1e82c5d`.
- Root source/pins acceptance: `8ce3f530afb7e38c15caca0cd8e84a873bd0a355`.
- Image: `acore/ac-wotlk-worldserver:solo-strategies-20260919-191935`, also `:master`.
- Image ID: `sha256:ceef54dc2c9c511651e5fd63d0597490fd452e2f501f8b250e2645277b33ca56`.
- Running binary SHA256: `a4d5b9d7ba4760112564be17ec7fa72fa2a343f65445dca6f05f62969ca9f386`.
- Rollback tag: `acore/ac-wotlk-worldserver:pre-solo-strategies-20260919-191935`,
  image `sha256:e73828a3918964750d0d0ca91b49aad7459d928bbe3c0424b39c412b10d6b959`.
- Private evidence: `backups/world-bot-solo-deploy-20260919-191935/`.

The native build/link ran 19:22:28–19:34:51 local, using the same pinned Ubuntu base
and build arguments as the previous image, without touching the running server.
The built binary contains the repair, and its disassembly confirms
`PlayerbotRepository::Load` precedes `RestoreWorldBotSoloStrategies` in the login path.
Runtime dependency resolution passed in an isolated, networkless `ldd` check with
no server entrypoint or database connection.

All 5,016 source/Git manifest records and 48 config/policy records remained unchanged
across build and deployment. Compared with the previous deployed source, non-Git
build-input differences were exactly the three intended playerbot files plus two
previously published module `AGENTS.md` files. Custom module mirrors matched.

Deployment used:

```sh
docker compose up -d --no-deps --no-build --pull never --force-recreate ac-worldserver
```

No setup/update script, importer, helper startup, migrations, gameplay SQL edits,
bot commands, level sync, gear rerolls or policy/config changes were performed.

## Preservation

- Fresh four-database dumps before building and after clean shutdown were verified.
  Stopped-state checksums include saves, inventory, levels/XP, roster, quests and profiles.
  This latest stopped state—not an older raid/character snapshot—is the recovery baseline.
- No human account was online immediately before shutdown. The old server exited cleanly.
- Stopped/start levels and XP, quests, saved strategy profiles, roster, and save/bind/deadline
  snapshots matched exactly. Instance/bind/reset tables were already empty at this restart;
  no historical raid snapshot was restored.
- **Inventory exception:** four new Guild Tabards (5976) appeared in ordinary bag slots
  for Pilbok, Kaaren, Keilmere and Feelesia. Existing `OnBotLoginInternal -> InitGuild`
  supplies a missing tabard to guild members. Every pre-existing inventory/item row,
  including equipped gear, was unchanged; no items were removed or replaced.
- Auth, database, importer and client-data container identities/states/start times remained
  unchanged, as did Pi bridge PID/invocation/start time. Bridge `/api/tags` remained healthy;
  no generation request was made.
- Core updater remained disabled. The independent playerbots updater remained enabled;
  all 28 applied SQL entries matched included source exactly and both ledgers were unchanged.
- Player and random-bot caps remain 70; seven GUID opt-ins, nine bracket exclusions,
  new RPG and installed raid policy bytes remain unchanged.
- The new worldserver remained running with restart count 0. Existing action-button and
  malformed vendor-data diagnostics also occurred in the old server's shutdown log;
  this deployment did not attempt unrelated data repairs.

## Initial live behavior — beyond merely being online

All seven selected bots, plus Ari/Meliah, logged in and remained ungrouped in the
observed snapshots. Redshift remained offline with unchanged level/XP. Human-online
checks during observation were zero. No summons, invitations, bot commands or DB
writes were used to stimulate activity.

The five previously stalled characters showed these saved XP values:

| Bot | Stopped/start | 19:42 local | 19:47 local | 19:52 local |
| --- | ---: | ---: | ---: | ---: |
| Ailina | 0 | 1,860 | 10,356 | 16,392 |
| Pilbok | 1,680 | 1,680 | 23,886 | 23,886 |
| Kaaren | 0 | 0 | 5,130 | 26,832 |
| Keilmere | 0 | 9,600 | 19,200 | 26,580 |
| Feelesia | 0 | 0 | 0 | 0 |

All five remained level60 during this interval. Ailina, Pilbok, Kaaren and Feelesia
accepted new Hellfire quests rather than retaining only their old AQ ring quest.
**Pilbok also gained rewarded-quest rows for 9390, “In Search of Sedai”, and 9423,
“Return to Obadei”,** then picked up “Cruel Taskmasters” (9399). These rows were absent
at shutdown/startup and appeared during ordinary solo activity after the fix.

This establishes resumed autonomous activity and actual quest turn-ins, not just
login or scheduler teleportation. XP was not individually traced to kills versus
exploration/quests. Feelesia accepted quests but had not yet gained XP in the initial
16-minute observation; Keilmere gained XP without a new saved quest in that interval.
Sustained progression for every bot through level70, and live invitation/manual-order
behavior, remain to be observed. The source regression suite covers those control gates;
a build or a short activity window is not proof of the entire leveling journey.
