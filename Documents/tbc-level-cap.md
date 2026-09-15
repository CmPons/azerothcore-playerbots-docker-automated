# Level70 cap — pending activation

On September15 the user requested a level70 cap to start Burning Crusade content.
Configuration is updated; **the running server still has the old cap until an
explicitly authorized worldserver restart**. No image rebuild is required.
`WorldConfig.cpp` marks `MaxPlayerLevel` as non-reloadable.

Changes only:

| File | Setting | Before → after |
|---|---|---|
| `.env` and `azerothcore-wotlk/.env` | `MAX_PLAYER_LEVEL` | 60 → 70 |
| Same env files | `RANDOM_BOT_MAX_LEVEL` | 60 → 70 |
| `azerothcore-wotlk/env/dist/etc/worldserver.conf` | `MaxPlayerLevel` | 60 → 70 |
| `azerothcore-wotlk/env/dist/etc/modules/playerbots.conf` | `AiPlayerbot.RandomBotMaxLevel` | 60 → 70 |

Both env copies and runtime configs were edited directly; no broad setup/apply,
reload, restart, database write, level grant or gear rewrite was performed.
Existing random-bot population management can use the higher cap after activation;
its guild/friend/name exclusions and other settings were left intact.

`Expansion = 2` remains unchanged (the existing WotLK server already supports BC).
XP rates, raid scaling, saves/binds/reset deadlines, policy, playerbot gear/enchant
expansion settings and all services are unchanged. Auction-house stock still has
its separate level60 limit; that was not included in this cap change.

Private before-files and verification:
`backups/level-cap-70-20260915-210600/`. Env files contain private values and remain
untracked; do not publish them.

On restart approval: take fresh progression/character/config backups, check logouts,
stop/restart worldserver only, and compare against the latest stopped state. Do not
restore an old raid snapshot or assume a previous restart authorization still applies.
