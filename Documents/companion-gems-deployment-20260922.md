# Companion gem deployment — September 22, 2026

## Result

User authorized deployment. Worldserver was built, cleanly stopped and recreated **once**;
ready at **2026-09-22 14:48:03 UTC**. Gem-only maintenance is enabled and all nine online
companions returned. No human players were online immediately before the stop.

At 14:48:09 UTC the new system filled Arinerica's five empty sockets. The ordinary
five-minute character save persisted:

| Equipped item | Added gems |
|---|---|
| Hope Bearer Helm | Three **Solid Stars of Elune** (24033, rare) |
| Gauntlets of Desolation | One **Solid Empyrean Sapphire** (32200, epic), one **Solid Star of Elune** |

These are stamina gems appropriate to her tank role. Previously occupied sockets and
permanent enchants across the monitored characters—including Redshift's existing gems—
were not changed. Only one gem-fill log was emitted through the first periodic interval;
no repeated socket writes were observed.

**There were unrelated post-restart state changes, including a saved-profile loss; see
Preservation exceptions below. This was not an exact all-state-preserving restart.**

## Build and controls

- Feature source: root `947f30e41478d88872a2bae9f1a9c4fdd843c20c`.
- Core: `897c2c6d72632ad6e3f1c01a9e82815ce2c165a4`.
- Playerbots: `693840886d1db462c141f4d31cbb606eb96b8a84`.
- Image: `sha256:774dc6fa19f2d20cc0356ada9973cf9fa4cb1b7ffdafcbbc6c951c071c5f9207`.
- Tag: `acore/ac-wotlk-worldserver:companion-gems-20260922-164217` (also `:master`).
- Running worldserver SHA256: `6bf3486c6a0611da633672b83480988a391c3907a5fba17fc96540fb410f459a`.
- Container: `9eadf27aee96b9269c0073135f590886001af944007f9f2e069cfc9e3ed49c93`.
- Container started: `2026-09-22T14:47:39.971089003Z`; restart count 0, no OOM.
- Rollback image retained: `acore/ac-wotlk-worldserver:pre-companion-gems-20260922-164217`,
  image `sha256:e77033aec686e3671691f037b42e311e2aa914f1c7cd22e8834b8fac6cc1f0e7`.

The build ran while the old server continued serving. All 29 targeted tests passed.
Native symbol inspection proved the new update/equip/store/startup hooks and gem functions
were linked; loader disassembly proved registration. Runtime dependencies resolved, and
the image binary matched the running container binary.

Source manifests contained 5,065 records. The delta from the previous running build was
limited to the three new gem files, raid-roster loader and config template. Root/native
module mirrors matched. Native fork revisions and database updater ledgers did not change.

Fresh four-database backups were verified both before building and after the clean stop.
Only worldserver was recreated, using `--no-deps --no-build --pull never --force-recreate`.
Authserver, database and helper container identities/start times were unchanged. Pi bridge
PID/invocation remained unchanged. No setup/importer run, gameplay SQL writes, bot commands,
respecs, roster syncs, forced saves, extra restarts or forced bot relogins were performed.

## Active configuration

Exactly these four keys were added/set in the runtime `mod_raid_roster.conf`, with matching
reproducible knobs in both private `.env` files:

```ini
CompanionMaintenance.SocketGems.Enable = 1
CompanionMaintenance.SocketGems.IntervalSeconds = 300
CompanionMaintenance.SocketGems.MinLevel = 61
CompanionMaintenance.SocketGems.EpicPercent = 20
```

Other runtime configuration/policies were unchanged. Level70 caps, companion protection
cap15, selected world-bot GUIDs, AH settings and chatter settings were retained.

## Preservation and exceptions

Snapshots covered **43 characters** (the roster/friend union plus Redshift), including
**1,747 item instances**. Stopped/immediate-start item rows, inventory placements, levels/XP,
roster, quests and saved profiles matched exactly. Talents and learned spell lists also
matched through the later persistence observation.

Through the five-minute persistence observation:

- All 1,747 item identities and inventory placements remained intact; no items were added,
  removed or replaced in the monitored inventories. Five previously empty socket enchantments
  changed as intended. No previously installed gem or permanent enchant was overwritten.
- Three existing temporary weapon-enchant durations counted down normally.
- Four existing BOP trade-window flags expired (257 → 1): Arinerica's gloves and three
  Raney items. The gem routine itself refuses trade-protected items and does not clear flags.
- Levels/XP, roster, talents and spellbook entries remained exact.
- **Existing offline-party cleanup:** LFG group7846 (Redshift, Arinerica, Meliah, Raney,
  Beliona) dissolved on bot login with `KeepAltsInGroup=0`. Completed normal Shattered Halls
  save3877/map540/difficulty0/mask7 and its five nonpermanent binds were cleared. The stopped
  snapshot contained no raid or permanent binds. This caveat was reported before the stop.
- **Six skill rows normalized during login, before the first gem pass:** Beliona's Herbalism
  maximum340→375; Ailina's Alchemy/Herbalism maxima325→375; Feelesia's Herbalism maximum345→375;
  Pilbok/Feelesia Dual Wield1/1→350/350. Profession skill values themselves did not increase.
  Existing core skill/spell loading reapplies level ranges and learned profession ranks.
- **Unresolved saved-profile loss:** Raney's 14 saved `playerbots_db_store` rows were present
  immediately after startup, but absent by the five-minute observation. The other saved
  profiles remained unchanged. Her talents, spellbook, level and gear did not change.
  The gem module has no profile writes or strategy resets. Existing `ResetAiAction` handles
  group-list events and calls `PlayerbotRepository::Reset`, which deletes saved rows; this
  is a likely path following group cleanup, **not conclusively proven by an action trace**.
  The fresh stopped backup retains the exact rows. No profile restore or speculative live
  strategy reset was attempted; recovery and any durable fix need separate approval/review.
- Raney accepted quest10416, *Synthesis of Power*, during subsequent autonomous activity;
  other quest data was unchanged. No operator quest or XP changes were made.

Nonfatal startup diagnostics included action-button cleanup for other bots and existing
priority/config/vendor warnings. No fatal error, crash loop or gem-module SQL failure was
observed. These diagnostics were retained, not broadly claimed identical to previous logs.

## Evidence and remaining acceptance

Private evidence: `backups/companion-gems-deploy-20260922-164217/`.
It contains dumps, config backups, stopped/start/persistence snapshots, source/config
manifests, build and startup logs, binary checks, `item-verification.json`,
`deployment-closeout.json` and checksums. It is not committed or publicly uploaded.

Confirmed live: initial catch-up, actual blue/epic provision, normal database persistence
and occupied-socket preservation; no repeat fill was logged past the first periodic interval.
The no-op periodic pass has no dedicated telemetry. No forced gear upgrade, meta-equipped
loadout, combat/trade trial or extra relog was performed solely for testing; those cases
currently have offline fixture coverage rather than live acceptance.

Keep the fresh stopped backup for scoped recovery. Do not restore an old full gameplay
database over subsequent progress to undo a profile loss or generated gems.
