# Kael mind control: small-raid balance and existing bot counter repair

## Scope

These are separate changes, authorized October 9, 2026. They do not change
phoenix assignments, advisor health, encounter timers, tank authority, generic
healing/threat controls or gear valuation. The bot work repairs the existing
native Infinity Blade mechanic; it adds no new encounter assignment policy.
Future encounter coordination remains subject to the Lua-policy requirement.

### Target-count balance

`mod-raid-scaling` caps Kael's spell **36797** at **one target per cast** when
scaling is enabled for map 550 with original size 25 and configured target size
1–10. Actual attendance is not used. Unscaled/disabled instances and configured
sizes above ten retain the native up-to-three targets.

```ini
RaidScaling.KaelthasMindControlTargets = 1
```

The new dist-template setting accepts 0 (no cap), or 1–3. Invalid values fall
back to 1; absence uses 1 without a missing-option warning. Configuration reload
applies to subsequent casts. An existing smaller per-cast cap is never raised.
The native boss script still requests three, and its random selection, actual-
victim exclusion, LOS filtering, threat reset and timers remain unchanged.

**Duration remains 30 seconds.** Live `Spell.dbc` and `SpellDuration.dbc` hashes
match the inspected client data: spell36797 duration index9, row
`(9, 30000, 0, 30000)`. The count limit does not guarantee that successive waves
cannot overlap: the native repeat interval is shorter than the aura duration.
The Infinity Blade counter still matters.

The module uses `OnSpellCheckCast`: core `Spell::prepare` checks casts before
instant execution, whereas `OnSpellPrepare` occurs after that instant-cast path.
No SQL or core boss-script modification is required.

## Repaired bot chain

Source inspection and executable production-code fixtures identified defects,
not a retrospective proof of exactly which one occurred in the recorded wipe:

1. **Loot approach:** the previous action could stop inside the configured
   15-yard loot radius but outside `OpenLootAction`'s three-yard admission.
   Approach now uses the smaller valid radius and moves to two yards.
   A failed earlier weapon opportunity no longer prevents checking later ones.
2. **Required weapon hand:** possession alone was insufficient. The existing
   ability choice is paired with the actual equipped Infinity Blade (30312):
   combat rogue Shiv/offhand; other rogues Sinister Strike/mainhand; hunter
   Wing Clip/mainhand; enhancement shaman Stormstrike/mainhand; non-Arms
   warrior Hamstring/mainhand. Existing class/spec loot eligibility is unchanged.
3. **Equipment admission:** owned equipment/inventory only, not the bank;
   unbroken dagger; core `CanEquipItem` and `SwapItem` preserve displaced items
   and normal restrictions. No items are created/deleted and no stats are
   substituted. The loot action equips the designated hand, and rescue
   rechecks/repairs it before movement or a cast.
4. **Discovery:** native MC resets threat. Rescue no longer requires the bot's
   threat-based `find target` lookup to rediscover Kael first. It requires a
   nearby living in-combat Kael, a living in-world uncontrolled rescuer, and a
   living hostile group member on the same map with **Kael-cast** spell36797.
   It excludes self and Kael's current victim, retaining nearest-target behavior.
5. **Admission:** rescue is a `MovementAction`, not an `AttackAction` falsely
   treated as damage against the current boss target by generic threat gates.
   This is not a blanket healing/damage exemption. Normal movement, ability,
   range, LOS, resource and cooldown checks remain; no boss attack is issued.
6. **Cast/proc/removal:** only the matching rescue ability is attempted after
   the dagger is ready and the member is within melee reach. The ordinary
   item proc performs the dispel; the bot does not directly remove the aura.

Infinity Blade's built-in on-hit spell is **36478 Magic Disruption**, configured
at **60 PPM** in the live item template. Its effect0 is dispel (38), misc8, one
charge, matching MC's dispel type8. Native equipped-hand dispatch and the
successful non-damaging-melee path support Wing Clip; Shiv triggers its native
5940 attack. No proc rate, attack-hit behavior, dispel resistance or item data
was changed.

## Read-only diagnostics

The existing local bot command server accepts `tkmc,<bot-guid>`. It reports
ownership, actual main/offhand entries, required hand, ability/known spell,
readiness, sampled group MC remaining durations and eligible rescue target.
It performs no gear/strategy/gameplay command.

`[PlayerbotTKMC]` server logs record cast submission and whether MC is still
present immediately afterward. **Action OK or submitted=true is not proof of
removal.** A submitted cast with MC still present is explicitly not reported as
a break; asynchronous events and later aura expiry need subsequent observation.

## Validation and limits

`scripts/tests/test_tk_mind_control.py` extracts the actual production scaling
hook, helpers, loot/rescue actions and trigger into C++ game-service doubles.
It covers small/native/disabled scope and invalid settings; correct/wrong/bag/
broken/unusable hands; displaced gear; control, boss-victim, caster, group and
map guards; discovery after threat loss; movement/cast failure and aura state;
plus old-code reproductions, mutants and ASan/UBSan.

A separate fixture executes native `Player::CastItemCombatSpell` equipped-hand
dispatch, its built-in item-proc prefix (not the enchantment branch), and
`Spell::EffectDispel`. It asserts actual aura-removal requests for spell36797,
wrong-hand rejection and failed admission/resistance retaining the aura. RNG,
packets, spell lookup and aura storage are service doubles; this is **not a
full-world integration test or live encounter acceptance**.

The small spell-data fixture records inspected live data and its hashes; no
DBC binaries or private character data are published. The prior weapon-recovery
suite now preserves unchanged loot *eligibility* rather than requiring the
intentionally repaired loot execution body to remain byte-identical. Its actual
recovery/backoff coverage remains, and new loot behavior has dedicated tests.

Private investigation evidence: `backups/kael-mc-work-20261009-212842/`.
Deployment and preservation results are recorded separately after verification.
