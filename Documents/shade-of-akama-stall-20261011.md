# Shade of Akama: stale channel slow stacks — October 11

## Finding

The live Shade retained **96 stacks of Shade Soul Channel (40520)** after the
hostile channel auras were gone. This is enough to reduce movement to zero. The
encounter stops its add generators only when the Shade reaches Akama, so the
stale slow prevents the transition and leaves the raid fighting repeated adds.

This is not a required wave count or evidence of insufficient raid DPS. Source
inspection identifies an incompatibility between the existing channeler script's
casting-state guard and newer core handling of action-allowed channels.

No gameplay code, configuration, database state or services were changed by the
assistant. This task investigated the running encounter and documented the cause;
a permanent fix has **not** been implemented or deployed.

## Direct evidence

The user supplied a `.list auras` screenshot for creature **22841**, low GUID **26**:

| Aura | Stacks | Recorded caster |
| --- | ---: | --- |
| Vertex Shade Black, 39833 | 1 | Shade 22841 / low 26 |
| Akama Soul Channel, 40447 | 1 | Akama 23191 / low 345 |
| Shade Soul Channel, 40520 | **96** | Channeler 23421 / low 31 |

Only those three auras were listed: no hostile channel aura **40401** remained in
that snapshot. Akama's **40447** is expected; removing it is not the proposed recovery.
The caster GUID on the residual slow records its source, not proof that this caster
was still alive. The user reported no hostile channelers remaining.

Read-only DB observations of BT **564 / instance 711**:

- Initial: completed mask **3**, serialized `B T 3 3 1 0 0 0 0 0 5 0 `.
  Naj'entus and Supremus done; Shade in progress.
- After the user's report of getting past the bug: mask **7**, serialized
  `B T 3 3 3 0 0 0 0 0 5 0 `. Shade done.

A bounded remote bot capture produced **54 responses**, three sequential rounds
across nine bots, **20:47:35–20:48:09 CEST**, with no query errors. It finished at
20:48:11.968; no collector was left running. Bots fought newly generated defenders,
elementalists, rogues and spiritbinders. Brief sorcerer presence also appears in
some nearest-NPC snapshots; that is not a full census or proof of a live channel.
These cached/sequential bot observations cannot reconstruct the Shade's aura history.
The user screenshot provides the decisive direct aura evidence.

The subsequent user reports were “Odd fight. Easy when we get past the bug,” then
explicitly **“.unaura 40520 worked fine.”** The user therefore confirmed the targeted
aura-removal recovery, rather than needing the proposed force-kill. This agrees
with the observed DONE state and strongly supports the stale slow as the immediate
blocker. The command and movement transition were not independently recorded as
server events. This remains a **GM-assisted recovery**, not an unassisted kill.

## Source-level failure chain

1. `npc_ashtongue_channeler::Reset` schedules a callback every **3.6 seconds**:
   ```cpp
   if (!me->HasUnitState(UNIT_STATE_CASTING))
       me->CastSpell(me, SPELL_SHADE_SOUL_CHANNEL, true);
   ```
   The channel is **40401**. The original channeler implementation is from core
   commit `dc7753b2b6` (April 2024), not a local bot or raid-scaling change.
2. Installed Spell.dbc gives 40401 `AttributesEx=4` and `AttributesEx5=1`:
   `SPELL_ATTR1_IS_CHANNELED` plus `SPELL_ATTR5_ALLOW_ACTION_DURING_CHANNEL`.
   Consequently `SpellInfo::IsActionAllowedChannel()` returns true.
3. `Unit::SetCurrentCastedSpell` now sets `UNIT_STATE_CASTING` for a channel **only
   if it is not action-allowed**. The actual channel remains in
   `CURRENT_CHANNELED_SPELL`, but the old script's state-bit guard does not see it.
   This conditional arrived in upstream core commit
   `1eb3749b3b376391c20f31377467aa858b2b9a99` (July 21, 2026), PR **26710**:
   “Creatures will not interrupt channeling spells anymore on engage if the spell
   has attribute ‘Allow Actions During Channeling’.”
4. The callback therefore submits another 40401 while the same channel is active.
   `SetCurrentCastedSpell` recognizes replacement by the same spell as `bySelf`;
   `Spell::cancel(bySelf)` skips removal of the old owned channel aura in that case.
5. Each 40401 cast's second effect triggers **40520**. SpellMgr gives 40520
   `SPELL_ATTR0_CU_SINGLE_AURA_STACK`, so stacks from different channelers share one
   aura. Installed data permits **99 stacks**. Refresh/stack handling increments it.
6. When a hostile 40401 channel aura is actually removed, the encounter's
   `spell_shade_of_akama_shade_soul_channel::HandleEffectRemove` subtracts **one**
   40520 stack. It does not subtract the extra applications accumulated by recasts.
7. 40520 is `SPELL_AURA_MOD_DECREASE_SPEED`, base -31 with die-side 1: **-30% per
   stack**, permanent duration. At 96 stacks the resulting negative movement rate
   is clamped to zero by `Unit::SetSpeed`.
8. The Shade's scheduled movement requires `GetSpeed(MOVE_WALK) > 0.01f`.
   Its `MovementInform(POINT_MOTION_TYPE, POINT_ENGAGE)` callback is where generators
   stop and the Shade becomes selectable to fight Akama. Zero speed prevents that.

The relevant installed `spell_script_names` binding is present; no `spell_dbc` or
`spell_linked_spell` overrides were returned for 40401/40447/40520. The script names
for Shade, Akama, channelers and sorcerers match their native implementations.

This establishes the faulty guard and a source-consistent route to the observed
stale stacks. The investigation did **not** trace every historical recast or replay
the complete encounter in a test server. A small arithmetic/flag sanity check was
performed; it is not a production encounter regression test.

## Recovery versus permanent repair

The user was considering `.damage 1000000` to escape the fight. After saving the
screenshot and capture, a narrower optional GM intervention was suggested:

```text
# With the Shade selected, after hostile channelers/sorcerers are dead:
.unaura 40520
```

Core `HandleUnAuraCommand` removes only that spell's auras from the selected unit.
The user subsequently confirmed that this worked. It recovered the attempt without
the proposed direct boss kill. Do **not** use `.unaura all`, remove Akama's 40447, or
treat a GM recovery as a clean progression test. The assistant issued no GM mutation
commands. Removing this aura does not repair the channeler script for future attempts.

The narrow permanent repair should make the channeler test the **actual active
spell/channel**, not only `UNIT_STATE_CASTING`. For example, the existing
`IsNonMeleeSpellCast(false)` checks an unfinished current channel even when actions
are allowed. Validate the final choice against initial casts, repeated callbacks,
legitimate interruption/rechannel, deaths, sorcerers, wipe/reset and repull behavior.
Do not globally change action-allowed spell semantics or simply disable the slow
mechanic. Any reconciliation of already-stale stacks needs separate, fail-closed
handling that preserves genuine active channel contributions.

The earlier Aqueous Lord/BT-strategy DPS complaint remains a **separate uninvestigated
issue**; this finding does not identify its cause. The flight-casting and TK policy
changes were not involved in this NPC channel guard.

## Files and provenance

Relevant native files:

- `src/server/scripts/Outland/BlackTemple/boss_shade_of_akama.cpp`
- `src/server/game/Entities/Unit/Unit.cpp`
- `src/server/game/Spells/Spell.cpp`, `SpellInfo.cpp`, `SpellMgr.cpp`, `SpellEffects.cpp`
- `src/server/shared/SharedDefines.h`, `DataStores/DBCStructure.h`
- `src/server/scripts/Commands/cs_misc.cpp`

Core revision: `7ecb74c2a3c95bf1aa83e38a0e3ca197beb6cb72`.
Playerbots: `1e7ee6b890767da1f3ff517b2eb97eab9bf06054`.
Worldserver image remains
`sha256:6828e979ae51dddc2a55903d22d77f4ea5695e02e9a9ab7393d4d26cdefe1e46`,
started `2026-10-11T10:00:42.741911913Z`, running with restart count zero.

Private evidence: `backups/shade-investigation-20261011-204633/`, pointer
`/tmp/shade-investigation`. Includes screenshot, raw capture, installed data,
source blame/change evidence and DB before/after observations. An initial DBC
summary used the wrong stack-field offset (70 rather than 49); it is retained
under an explicitly named initial-error file, with the corrected **99** in
`shade-spells.json`. No raw DBCs or private capture data are published.
