# Grouped hunter pets yield to tank-spec aggro

## Status

Implemented in source; **not built/deployed**. No live pet settings, configuration,
SQL, server process or Pi bridge were changed. Deployment needs fresh permission.

## Policy

For hunter pets owned by a grouped hunter, block Growl (all ranks) and spells with
true taunt effects when the spell's target is currently attacking a living,
in-world **tank-spec player in that same party or raid**. Both human and bot
hunters/tanks are covered. Subgroup and explicit MT assignments do not matter.

The actual current victim is authoritative. If absent, use the current victim
from an available threat list. Do not substitute a highest-threat guess or an
unrelated tank elsewhere in the group. Reuse `PlayerbotAI::IsTank(player, true)`:
Protection warriors/paladins and the project's existing druid/DK spec/form rules.

Allow the cast normally when:

- The hunter is ungrouped (solo behavior unchanged).
- No target/victim is resolved, the victim is a non-tank, another pet/NPC, dead,
  out of world, or outside the owner's group.
- The caster is not a hunter pet owned by a hunter.
- The spell is ordinary damage/control rather than Growl or a true taunt.

This supersedes the earlier proposed "only when both tanks are dead" rule. Pets
may help with a loose mob attacking a healer even while both tanks are alive.
They may still generate normal threat through damage; this does not forcibly
transfer a boss already tanked by a pet back to a player.

## Implementation

`src/Script/PlayerbotsHunterPetTaunt.cpp` adds a database-independent
`AllSpellScript`, registered from `src/Script/Playerbots.cpp`.

- `OnSpellCheckCast`: rejects protected-target casts with `SPELL_FAILED_DONT_REPORT`.
  Core pet autocast goes through `CanAutoCast -> CheckPetCast -> CheckCast`, so the
  ability is excluded from the candidate list instead of repeatedly being picked.
- `CanPrepare`: rechecks the actual cast after explicit-target initialization,
  covering direct commands/triggered casts and a victim change since candidate
  selection. A rejected preparation finishes unsuccessfully before effects occur.
- No changes to persistent autocast flags. The existing bot routine may continue
  enabling Growl, but cannot bypass the cast gate. No remembered state needs to be
  restored on target changes, tank deaths, regrouping or returning to solo play.
- Growl's first-rank ID is 2649. Wrath Growl uses a flat-threat effect rather than
  the true taunt effect, so merely checking `SPELL_EFFECT_ATTACK_ME` is insufficient.
  Also cover `SPELL_AURA_MOD_TAUNT`; ordinary Bite/Claw/Thunderstomp/Intimidation are
  not suppressed.

No SQL registration, core changes, configuration option or setup changes are
required. Canonical reproduction is
`patches/0021-playerbot-hunter-pet-taunt-policy.patch`, applied by the existing
setup/update patch loop. The module lives in the nested playerbots checkout;
there is no separate root `modules/mod-playerbots/src/` copy to synchronize.

## Verification

```bash
python3 -m unittest discover -s scripts/tests -p test_hunter_pet_taunt.py -v
```

Four tests passed: compile and execute the **complete production hook** with C++20
API doubles and warnings-as-errors; core lifecycle/registration source contracts;
no autocast-setting mutation; and patch reverse/apply idempotence/round-trip.
Behavior cases include all nine Growl ranks, true taunts, solo/group transitions,
human/bot tanks and owners, other groups, dead/offline/non-tank victims, ordinary
pet spells, threat fallback, an aggro change between selection and preparation,
and preserving a previous cast error.

These are offline tests, not a full server compile or live-cast verification.
After authorized deployment, validate actual Growl casts while Redshift/Ari hold a
mob, healer-aggro rescue, target changes, group departure and solo hunter combat.
