# Raid creature scaling eligibility

## Policy

Damage and health now have separate eligibility. No new encounter entry allowlist was added.
Existing fixed-ten/default/manual/off policy and multiplier formulas are unchanged.

- **Damage:** an actual damage event from a raid creature to a player or a player-controlled
  victim uses the existing boss/trash multiplier, regardless of creature rank, trigger status,
  or attackability. Critters, civilians, player-origin/controlled sources and sources friendly
  to the victim are excluded. Neutral is not synonymous with friendly. Non-player-controlled
  creature victims retain unscaled scripted-event damage.
- **Health:** the existing linked Twins bugs and elite/boss paths remain, including bosses
  that begin an intro friendly or unattackable. New normal/rare-ranked bodies require native
  `IsHostileToPlayers()` faction evidence, and must not be triggers, critters, civilians,
  player-origin/controlled, immune to players, nonattackable or unselectable. Harmless utility
  triggers do not acquire health scaling. The existing Twins base-unitmod/mutation logic is
  unchanged. Damage eligibility never depends on this health predicate.
- **Classification:** normal adds/hazards use trash settings; dungeon/world bosses retain boss
  settings. An unattackable damaging hazard can therefore get damage scaling without HP changes.

The conservative new health gate was explicitly approved: neutral/reputation-based normal
creatures are not newly health-scaled, even when combat-capable. No attendance scan or deferred
combat/intro hook was introduced. Existing elite/boss health behavior is retained intentionally;
that legacy path is not a new promise to filter every friendly elite or intro helper.

## Ownership and native attribution

`RaidCreatureEligibility.h` walks owner, charmer, creator and temporary-summoner GUIDs, including
GameObject owners. It checks direct player GUIDs even if their objects cannot be resolved,
`IsControlledByPlayer()`, `IsCreatedByPlayer()` and pets. NPC-owned encounter summons are not
blanket-excluded as guardians. Current control checks run at use time, not only at spawn.
The walk is bounded to 16 objects per ancestry path and 64 visited nodes; cycles/overflow fail
closed. Duplicate links on one object are followed only once. Pointers are local to the call.

A single ephemeral `Origin` flag in native `Object::CustomData` captures excluded summon ancestry
at the existing raid-instance `OnCreatureAddWorld`, even if scaling is disabled/not initialized.
It propagates cached ancestors and survives the parent's despawn without a global registry,
GUID-indexed cache, save change or removal hook. Native DataMap owns/deletes this object data.
In native `Map::SummonCreature`, summoner GUID is supplied to the constructor and `InitStats`
sets player-created/minion metadata before `AddToMap`. Creature AI initialization precedes the
module add-world hook. `InitSummon` runs later, so later script changes cannot be guaranteed to
appear in the spawn snapshot (current checks still apply while their ancestry is observable).

Missing NPC ancestors are not automatically excluded: Ouro despawns one second after creating
mounds. This preserves their damage scaling after he disappears. **Limitation:** player origin
behind a never-observed, already-missing NPC ancestor cannot be proven. Likewise ownership
changes after the snapshot that disappear before observation cannot be reconstructed. The cache
is deliberately not a persistent provenance/history system.

Native `Unit::DealDamage` calls the UnitScript hook with its attacker; direct spell damage passes
its caster through `DealSpellDamage`, and periodic damage passes its caster to `DealDamage`.
The module uses that supplied source, without inventing a replacement spell caster or changing
spell mechanics. Player-attributed/self damage remains outside creature-source scaling.

## Offline evidence and boundaries

Run from the repository root:

```sh
python3 -m unittest scripts.tests.test_raid_creature_scaling scripts.tests.test_twins_bug_scaling \
  scripts.tests.test_raid_scaling_default scripts.tests.test_twins_reset -v
```

The harness compiles extracted production manager/hook methods and the production ancestry
helper with native faction calculations, native DataMap, and the native map runtime container,
visitor and GUID lookup. Loaded summons use independent runtime GUIDs with spawn ID zero.
It retains native Twins health-aura methods. Creature objects/health storage, ancestry lookup,
control flags and the non-reputation reaction dispatch remain test doubles; this is **not** a
full native module build, worldserver execution, or live combat proof.

Tests cover normal-ranked mound damage (750 becomes 326 under the unchanged `pow(.25, .6)`
multiplier), Ouro Scarab 15718 and an arbitrary normal-add entry, health,
trigger/neutral/unattackable hazards, legacy elite/boss behavior, friendly/noncombat exclusions,
player/pet victims, ownership and ancestry despawn/cycle cases, settings and instance isolation.
The mound rank is grounded in the supplied read-only schema evidence; artificial flags/factions
exercise generic policy, not a claim to reproduce every live template field. Original baseline
code compiles but fails the mound and normal-health assertions. Restoring damage's health gate
or removing the origin snapshot also produces assertion failures. All 35 tests in the command
above pass, including the two new loaded-runtime-store regressions. All three additional Bug
Trio reset tests also pass after the parent authorized copying exact missing patch0022/0030
fixtures into the private snapshot for testing only. No
reset test or reset implementation was changed; those copied inputs are not integration changes.

## Loaded-summon correction after review

Map-level apply/restore now snapshot Creature GUIDs from native `Map::GetObjectsStore()` using
`TypeContainerVisitor`, then re-resolve each through `Map::GetCreature`. Unlike the spawn-ID
store, this includes loaded temporary creatures with spawn ID zero. It does not load grids,
retain pointers/iterators across callbacks, or add a registry. Manual HP overrides and scaling
off/on therefore reach the same summoned combat bodies as spawn-time scaling.

The regression follows native runtime-store semantics: 3052 original HP -> default 763, damage
to 381 -> manual 0.5 gives max 1526/current 762 -> disable gives 3052/1524 -> reenable gives
763/381. Damage from 750 remains 326 except while off (750), and normal base-unitmod stays
3052. It also covers off-state spawn, repeated/no-heal applies, alive low-health min-one
rounding, Corpse/Dead zero HP, and removal/destruction before and during traversal. The normal
restore path now uses the same dead-to-zero guard as the Twins path, without changing death
state. Retained reviewed spawn-ID-only walkers and a dead-min-one mutant fail the regression.
AddressSanitizer supplements UBSan for the removal/destruction cases.

Entry 15718 is Ouro Scarab, not a C'Thun eye tentacle; the small eye tentacle is 15726.
No encounter ID behavior changed for this label correction.

No mound count, speed, timer, health-transfer, encounter/reset, Lua/C'Thun, configuration,
persistence or native production source changes are part of this change. Deployment and any
required server restart belong to the parent/user; none was performed or authorized here.
