# World-bot and chatter deployment — September 19, 2026

## Active deployment

Explicitly authorized worldserver-only restart; ready at **2026-09-19 15:32:01 UTC**.
Auth, database, DB importer, client-data helper and Pi bridge identities/start times
remained unchanged. No dependency helpers were started.

- Source: guild/directional-friend chatter `2a58d97`; selected world bots `c6264a6`.
- Image: `acore/ac-wotlk-worldserver:roster-world-bots-20260919-170406` (also `:master`).
- Image ID: `sha256:e73828a3918964750d0d0ca91b49aad7459d928bbe3c0424b39c412b10d6b959`.
- Binary SHA256: `dd032fc9c4a03fb2ee9558cbd66c42e683394119c16217e1c0c440fac8ab7651`.
- Rollback image tag: `acore/ac-wotlk-worldserver:pre-world-bots-20260919-170406`
  (`sha256:84fd791315036dab9574e135b37fe9ca1dd526812f5b8b09b000e57e7f281ae2`).

Raney, Beliona, Ailina, Pilbok, Kaaren, Keilmere and Feelesia were all confirmed online
under the new setup, alongside Arinerica and Meliah. Use ordinary `/invite NAME` for
online bots. Login commands do not recruit already-online characters. No live command
was sent to force questing, gear changes, invitations or AI speech during verification.

## Configuration

Only four existing files changed: both private `.env` copies, mounted `playerbots.conf`,
and mounted `mod_player_bot_level_brackets.conf`.

```ini
AiPlayerbot.WorldBotGuids = "1118,1159,1180,1274,1297,1315,1433"
BotLevelBrackets.ExcludeNames = Meliah,Arinerica,Raney,Beliona,Ailina,Pilbok,Kaaren,Keilmere,Feelesia
```

Both env copies carry the same list in `BOT_LEVEL_BRACKET_EXCLUDE_NAMES`. Level caps
remain 70. Existing friends protection/max-five, ordinary equipment/refresh maintenance,
activity settings and cheats remain unchanged. No `.raidroster sync`, character-account
conversion or roster rewrite was performed.

## Evidence and preservation

Private local evidence: `backups/world-bots-deploy-20260919-170406/` (not uploaded).
Build evidence is in its `build/` subdirectory. Backups include actual layered source
and Git metadata, configs/env, policies/checker, rollback and staged images, and all
four databases both before deployment and again after the clean worldserver stop.
The final stopped dump—not historical raid snapshots—is the recovery authority.

- Exactly 11 source/doc/config paths changed from the previously deployed image:
  four playerbots files plus seven chatter paths. Git metadata and all other compiled
  Lua/encounter/equipment sources were unchanged. Parent recomputed the full manifest
  delta and verified all 4,749 final input records.
- Native compile/link passed; static ELF inspection verified the new symbols, Lua5.4.8/
  API2 and the 19-library runtime closure. No staged binary was executed before deployment.
  Independent source and build-evidence reviews found no issues.
- Zero non-random characters online immediately before the clean stop; exit0, no OOM.
  Recreated only worldserver with `--no-deps --no-build --pull never --force-recreate`.
  **Never use `docker compose start` for an existing-container world-only operation:**
  it can start dependency helpers. Use `docker start ac-worldserver` in that case.
- All saved instances, binds, progression deadlines and AQ respawns matched the stopped
  state exactly at startup. Ten-character levels/XP and saved roster rows also matched.
- Every equipped item and all other inventory/item rows matched except two Beliona
  conjured items: Major Healthstone9421 and Major Soulstone16896. The stopped dump shows
  she had been offline1,817 seconds before container start; core inventory loading removes
  conjured items after15 minutes. This normal expiry was recorded, not restored.
- Running binary matches the inspected artifact. Restart count0; core updater0;
  independently enabled PB updater's28 SQL files still exactly match its ledger.
  Auth/characters/world update ledgers were unchanged. No importer/migration run.
- Lua default revision remains
  `03f9e35d9345878b40a3beca5ea5e1a38cfad0221974711ec05bec9f5672b2f8`;
  checker/default and unrelated configs remained unchanged. Pi `/api/tags` was healthy;
  no generation was requested for operational testing.

`parent-deployment-closeout.json`, `inventory-startup-review.json`,
`STOPPED-SHA256SUMS` and `CLOSEOUT-SHA256SUMS` retain the detailed evidence.
Three build-checker assumptions (old policy revision, C++ ABI symbol decoration and
blanket config writability) were corrected; initial failures remain recorded.

Online status proves login, not completed quests, long-term leveling, invitation success
or conversation quality. Those remain gameplay observations. Normal world-bot maintenance
can replace bag supplies and apply existing attunement/reputation/talent behavior; this
is not a new loot-only progression system. Future restarts require fresh authorization.
