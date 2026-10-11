# Flight-aware casting deployment — October 11

## Result and scope

The explicitly authorized **worldserver-only** deployment is complete. Ready at
**12:01:03 CEST** (`2026-10-11T10:01:03.871294763Z`). The shared unit/destination
casting repair and bounded rejection diagnostics are live. See
[implementation and evidence limits](flight-aware-spell-casting.md).

The advisor timer, Gravity Lapse spreading, Lua targeting/ownership, threat
thresholds and MC balance were not changed. No live encounter was run to validate
the fix. The earlier flight veto is source-confirmed, but the October 11 capture
cannot attribute every heal/DPS failure to it; Kaaren's target gaps remain separate.

## Published and running identity

- Playerbots: `1e7ee6b890767da1f3ff517b2eb97eab9bf06054`.
- Root implementation/tests/pin: `b3644b7`.
- Core unchanged: `7ecb74c2a3c95bf1aa83e38a0e3ca197beb6cb72`.
- Image tag: `acore/ac-wotlk-worldserver:flight-casting-20261011-113944`.
- Image ID: `sha256:6828e979ae51dddc2a55903d22d77f4ea5695e02e9a9ab7393d4d26cdefe1e46`.
- Binary SHA256: `fa5d2f0547cd75d61103bb333f8b6a27bbf0546af6da064ec6451dc70b11af75`.
- Container started: `2026-10-11T10:00:42.741911913Z`.
- Retained rollback tag: `acore/ac-wotlk-worldserver:pre-flight-casting-20261011-113944`,
  the preceding October 10 threat/policy image. Rollback needs new interruption authorization.

Private evidence:

- `backups/flight-casting-work-20261011-111455/`, pointer `/tmp/flight-casting-work`.
- `backups/flight-casting-deploy-20261011-113944/`, pointer
  `/tmp/flight-casting-deploy-backup`.

## Validation and operational boundary

- **134 selected regression tests passed**, 396.588 seconds, no skips. This is not
  a claim that every historical repository test passes.
- Six new tests compile all four production casting overloads, diagnostic adapter,
  core inline flight/aura predicates and controlled doubles. Coverage includes
  ASan/UBSan, original-code failures, mutations, native-error injection and bounded
  concurrent observations. Two native-header syntax checks passed.
- Official C++ style checker: 62 existing diagnostics before/after, no additions.
- Full image build: 11:47:05–11:59:42 CEST. Sixteen compiler warnings, identical to
  the prior build after normalizing line numbers.
- Binary disassembly verifies all four shared-gate call sites, native `CheckCast`
  and `prepare` delegation, failure recording and the read-only remote handler.
  Earlier token, tank/pull, reset, scaling, loot, weapon recovery, threat, Lua
  ownership and MC fixes remain linked and covered by the regression suite.
- Fresh four-database/config/policy/checker snapshots; 4,215 frozen source records,
  exactly four intended native-file changes from the running source, mirrors match.
  All 50 config/policy/checker records remained unchanged through observation.
- Human-offline checks passed before preparation and immediately before stop.
  Fresh instance data contained no IN_PROGRESS encounter. Shutdown was clean
  (exit 0, no OOM). Only worldserver was recreated, using `--no-deps --no-build
  --pull never --force-recreate` after accepting the staged image.
- No setup/importer, SQL restoration, boss reset, threat override, Lua publication,
  runtime config edit, Pi bridge restart or unrelated service interruption.
  Other container identities/start times and bridge invocation remained identical.
- Read-only `castfail` queries on Ailina, Keilmere and Kaaren returned the new
  `CASTFAIL ... observed_failures=0` response. Idle zero counts are not live cast
  acceptance. No casts or pulls were issued by this verification.
- `prepare`, build, binary validation, deployment and infrastructure observation
  exited 0. Original early fixture extraction/constructor failures and a duplicated
  syntax-check path invocation are retained in the work evidence; corrected checks
  passed before building. No failed audit was overwritten or waived.

The unchanged Lua default remains publication `d503f67e6b49030d`, content
`8f322f0878e85fcf89c55ba82484891d68a56ff4c52b5829d7a32322658d8791`.

## Fresh raid-save baseline retained

TK instance **2510**, map 550, completion mask **15**, serialized `T E 3 3 3 3 `,
progression stage **3**, reset **1791777600**, extension **1791864000**. Instance
progress, deadlines, binds, global resets and saved gameobject state remained
exact across stop/start and the delayed-save observation. SSC's older save was
already absent from the fresh baseline; no historical tuple was restored.

These completion flags preserve the kill; they do not reclassify it as unassisted.
The user's possible GM Raney revive remains an uncertain report, not independently
confirmed or erased by this deployment.

## Preservation: infrastructure passed; strict audit remains failed

The managed scope is **43 characters**.

**Stopped → startup:** all **2,210 item identities**, inventory positions and
**727 equipped identities** were exact. Existing gems/permanent enchants, levels,
abilities, specs, roster, quests, profiles, pets and raid saves were exact.
Strict startup acceptance was false with one finding: group **3537** disbanded.
`KeepAltsInGroup=0` disbanding is the previously accepted behavior, but no audit
exception file was written.

**380-second observation:** strict acceptance remains **false**, seven findings;
`preservation-observation.exit=1`. Recorded qualifications:

- Raney lost **13 saved rows**: three strategy rows and ten values, including
  formation `chaos` and RTI `square`. This is the recurring, unresolved preference
  persistence issue, not fixed or restored here.
- Beliona's Master Soulstone `22116` identity `40790282` was replaced by `40802990`;
  Soul Shard `6265` identity `40097376` disappeared. Counts became **2,209 item
  identities**. The item types/pattern fit soulstone maintenance, but individual
  casts were not traced. No other item identities disappeared or appeared.
- All **727 equipped identities**, remaining item positions, existing gems and
  permanent enchants remained intact. Ten temporary-enchant durations changed:
  eight decreased; Kaaren's two same-ID weapon imbues (`2634`, `2636`) refreshed.
  No permanent enchant or socket change, stack/charge change, or gear move occurred.
- Three pets changed health/mana/happiness/save-time fields. Pet identities,
  levels, action bars, spells and autocast records remained intact.
- Levels, abilities, specs, roster, quests and all audited raid state stayed exact.

The seven strict findings comprise inventory, profiles, groups, pets, item
identities and two enchant refreshes. They were reviewed without converting the
strict audit to success, changing allowlists, restoring SQL or restarting again.

**Before → stopped, on the old runtime:** 2,211 → 2,210 item identities. A separate
Master Soulstone replacement and one Soul Shard removal already occurred in this
interval; five consumable/reagent stack counts and ten temporary-enchant durations
changed, plus pet save/happiness fields. Equipped identities, existing gems,
permanent enchants and raid state remained intact. Fresh stopped state was the
startup baseline, not a stale pre-build snapshot.

Runtime logs retain the process-priority permission warning and seven duplicate
`pet_spell` insert errors. Read-only owner mapping places all seven outside the
43-character managed scope; the same warning/error categories existed previously.
No assertion, crash, OOM, second readiness cycle or container restart occurred.
These observations do not establish a completely error-free server.
