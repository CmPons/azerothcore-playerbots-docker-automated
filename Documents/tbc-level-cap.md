# Level70 cap — active

On September15 the user requested a level70 cap to start Burning Crusade content.
**Active after the explicitly authorized restart at21:12 CEST.** No image rebuild
was required. `WorldConfig.cpp` marks `MaxPlayerLevel` as non-reloadable.

Changes only:

| File | Setting | Before → after |
|---|---|---|
| `.env` and `azerothcore-wotlk/.env` | `MAX_PLAYER_LEVEL` | 60 → 70 |
| Same env files | `RANDOM_BOT_MAX_LEVEL` | 60 → 70 |
| `azerothcore-wotlk/env/dist/etc/worldserver.conf` | `MaxPlayerLevel` | 60 → 70 |
| `azerothcore-wotlk/env/dist/etc/modules/playerbots.conf` | `AiPlayerbot.RandomBotMaxLevel` | 60 → 70 |

Both env copies and runtime configs were edited directly; no broad setup/apply,
reload, level grant or gear rewrite was performed. The initial configuration edit
did not restart anything; the later authorized activation is recorded below.
Existing random-bot population management can use the higher cap after activation;
its guild/friend/name exclusions and other settings were left intact.

`Expansion = 2` remains unchanged (the existing WotLK server already supports BC).
XP rates, raid scaling, saves/binds/reset deadlines, policy, playerbot gear/enchant
expansion settings and all services are unchanged. Auction-house stock still has
its separate level60 limit; that was not included in this cap change.

Private before-files and verification:
`backups/level-cap-70-20260915-210600/`. Env files contain private values and remain
untracked; do not publish them.

## Activation and preservation

Backup: `backups/level-cap-70-restart-20260915-211133/`. All four databases were dumped
before shutdown and again after a clean worldserver stop (exit0, no OOM). Source,
configuration, policy/checker and the current image were also backed up. A fresh
check confirmed zero online non-random characters immediately before shutdown.

The same worldserver container, image and binary restarted; readiness was observed
only in logs from the new start. `StartedAt = 2026-09-15T19:12:28.425286982Z`, restart
count0. Both mounted cap settings read70; source marks the player cap as startup-only.

Exact post-start comparisons passed for all saved instances/binds/deadlines/AQ respawns,
roster inventory/item rows, and roster levels/XP against the final stopped snapshots.
Auth/database/Pi service identities, configuration, Lua default/checker and binary were
unchanged. The independently enabled playerbots updater still matches all28 applied
files. The worldserver's core updater remains disabled.

### Unexpected dependency helper run

`docker compose start ac-worldserver` also started the existing client-data and DB-import
helper containers. This was unintended; use **`docker start ac-worldserver`** when starting
an existing stopped container without dependencies. No further restart was performed.

The client helper found data v20.0 already installed and downloaded nothing (it also
printed a missing optional module-manager include warning). The DB importer reported
Auth/World up-to-date, and executed one character SQL file:
`2026_09_05_00_playerbot_chatter_persona.sql`, SHA1
`3A09218284D9C8C5B878A01B807EDD9F7384C62F`.

The extracted statement was only `CREATE TABLE IF NOT EXISTS` for the existing chatter
persona table. Independent comparisons against the stopped dump verified its schema
and every dumped row unchanged. The importer added the corresponding `MODULE` entry
to `acore_characters.updates`; that metadata change was not rolled back. Private logs
and `helper-side-effect-review.json` preserve this deviation. Raid/gear/level comparisons
still passed; it is not accurate to describe this restart as having no DB writes.

Further restarts require fresh permission. Never restore an older raid snapshot.
