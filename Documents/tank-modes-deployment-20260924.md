# Tank modes deployment — September 24, 2026

## Result

The user authorized deployment. Worldserver was built while the previous server
remained available, stopped cleanly and recreated once. It was ready at
**2026-09-24 19:18:00 UTC**. All nine companions returned; no human was online at
the stop. No crash loop or OOM was observed through the ordinary-save review.

Live source changes:

- Cooperative `tank strategy mt | offtank | status`, group-role consistency,
  low-health MT pickup pause/help and best-effort square/diamond markers.
- Native unique-role persistence fix and party MT support.
- Maulgar's dedicated bot strategy removal; Gruul's own tactics retained.
- Chatter command filtering for the new command family.

**Startup again disbanded the saved group and removed Raney's saved AI profile.**
See the exceptions below. This was not an exact all-state-preserving restart.

## Provenance and rollback

| Component | Revision / identifier |
|---|---|
| Source root | `672185cf0602ec66ef7dffd5edbb24592075bc3b` |
| Core | `2e6dae2ce03a59e6b341ec63351e90b14c53b6f5` |
| Playerbots | `292669a0b536c3e988a31ec874996d85ee5dc2e2` |
| Image tag | `acore/ac-wotlk-worldserver:tank-modes-20260924-210908` |
| Image | `sha256:7ac8ea42fe4f68ecbabb984dbc4dc3cfe6baa5d26bf6de231b1dc2f8dc757d13` |
| Running binary SHA256 | `fce58e8b0b7c77c23e17237cac01ede806103f0e0504ebf2e5c0d25faa8a622a` |
| Container | `e85cc03c22a6927edf62a0f6d44c4c687206aac20ce8c1a2ba99558cbbcfbe8a` |
| Container start | `2026-09-24T19:17:46.200658979Z` |

Previous image retained as:

```text
acore/ac-wotlk-worldserver:pre-tank-modes-20260924-210908
sha256:774dc6fa19f2d20cc0356ada9973cf9fa4cb1b7ffdafcbbc6c951c071c5f9207
```

Image rollback would need fresh authorization and would not undo database/runtime
changes. Do not restore an old full database over subsequent earned progression.

## Build and deployment controls

- Four-database backups plus private env/config/policy backups were taken before
  building. A second verified four-database dump followed the clean stop.
- 27 focused tests passed. Previous native-header checks covered 16 integration
  translation units; the authorized full native build also succeeded.
- Source manifests covered 4,205 files, excluding Git internals. The exact delta
  from the previous running build was 19 files: two core, sixteen playerbots
  (including the pending GruulStrategy change), and the chatter classifier.
- Root/build-tree mirrors matched. All 48 config/policy manifest records remained
  identical through build, startup and the six-minute preservation check. No
  config application, setup, roster sync, respec or bot command was performed.
- The initial post-build symbol checker stopped because it expected `MarkRoles(`
  without the C++ ABI tag. Compilation had succeeded. The corrected validator
  found `MarkRoles[abi:cxx11]`, the correct `RaidGruulsLairStrategy` symbol, all new
  tank functions and retained companion/BG functions. Disassembly confirmed the
  group-role transaction path. Runtime dependencies resolved.
- The staged binary was copied from a never-started inspection container. Its
  hash matched the binary copied from the new running worldserver.
- Only worldserver was recreated:
  `docker compose up -d --no-deps --no-build --pull never --force-recreate ac-worldserver`.
- Auth, database and helper container identities/start times were unchanged. Pi
  bridge PID/invocation remained unchanged; `/api/tags` responded normally.
- Core and playerbots updater ledgers were unchanged. The enabled playerbots
  updater matched its source SQL exactly before/after; no importer was started.
- No direct gameplay SQL writes, forced saves, speculative restoration, extra
  restarts or forced bot relogins were used for validation.

## Preservation results and exceptions

Compared clean-stop snapshots against startup and the normal-save observation
six minutes after container start:

### Preserved

- **43 monitored characters; 1,994 item identities and inventory placements.**
  No equipment was added, removed or replaced in those inventories.
- Levels/XP, roster, quests, talents and learned player spellbooks were exact.
- All three saved raids and all **30 permanent binds** were exact:
  Karazhan5869/map532/mask727; Gruul6061/map565/mask3;
  Magtheridon6306/map544/mask1. Associated saved respawn/GO state also matched.
- Previously occupied sockets and permanent enchants were not overwritten.
- Other eight companions' saved AI-profile rows were unchanged.

### Observed changes

1. **Group12491 dissolved during bot startup.** It contained Redshift and the nine
   companions, with Redshift marked MT. This is consistent with the existing
   offline-group login cleanup with `KeepAltsInGroup=0`. No operator regroup or
   role assignment was issued. Regroup before testing the new commands.
2. **Raney's 14 saved `playerbots_db_store` rows disappeared again.** They survived
   the immediate startup snapshot, then were absent at the later observation.
   This is the same pattern seen on September22. Group-list `ResetAiAction` is a
   plausible path, not an event-traced conclusion. Her gear, talents, spells and
   level did not change. The new tank helper does not write AI profiles. The fresh
   stopped backup retains her exact current rows; no restore was attempted.
3. **Ten skill rows normalized during login:** First Aid/Cooking/Fishing maxima
   for Meliah, Ailina and Pilbok changed350→450 while values stayed350; Pilbok's
   Dual Wield changed1/1→350/350. No operator profession/skill commands were run.
4. **Existing gem maintenance filled ten empty sockets**, persisted by the normal
   save: Beliona3, Pilbok1, Ari3 and Meliah3. Ari's manually equipped Maiden gloves
   remain equipped and now have their two sockets filled. This is the previously
   deployed gem feature, not a new equipment change in the tank-mode code.
5. **38 BOP trade-window flags cleared** (257→1), and eleven existing temporary
   enchantment durations elapsed. No permanent enchant or occupied gem changed.
   The normal gem maintenance still refuses trade-protected items; it did not
   clear those flags itself.

The user was notified of the group/profile exceptions before closeout. Recovery or
a durable fix for that login behavior requires a separate scoped request, not a
whole-database rollback.

### Scaling caveat

No scaling config was edited. However, per-instance `.raidscale` manual overrides
are held in memory and do **not** survive a restart. The user was warned before the
stop that Mag's0.20 override would revert to the configured ten-player defaults.
No manual overrides were reissued. Raid completion/binds are separate and intact.

### Nonfatal diagnostics

Logs retain the process-priority permission warning, duplicate `pet_spell` insert
errors, and a rejected low-velocity movement spline for creature2974/GUID14044.
No fatal error/restart/OOM occurred. These were not broadly declared absent or
fixed by the tank-mode deployment. The three pet errors belonged to non-roster
Uridon/Gewa/Oran, not the protected companions; further details are in private evidence.

## Use and remaining acceptance

After regrouping:

```text
/w Arinerica tank strategy offtank
/w Arinerica tank strategy status
```

Use `tank strategy MT` when Ari should be MT instead. Role markers are opt-in via
the assignment command: MT blue square, OT purple diamond, without forcing occupied
icons away from encounter targets. Existing manual `stay`/RTI settings were not
blanket-reset by deployment; mode commands do not replace movement commands.

Startup, binary identity, ordinary persistence and the preservation exceptions
were verified. **Whisper routing, actual co-tank behavior, low-health help calls,
icon collisions and encounter swaps still need in-game acceptance.** No combat
success is inferred merely from a successful deployment. See
[playerbot-tank-modes.md](playerbot-tank-modes.md).

Private evidence: `backups/tank-modes-deploy-20260924-210908/`. It contains backups,
source/config manifests, tests, build/validation/deploy logs, native disassembly,
stopped/start/final snapshots, `audit-final.json` and checksums. Dumps, credentials,
live configs and binaries are not committed or publicly uploaded.
