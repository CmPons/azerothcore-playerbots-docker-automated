# Per-instance playerbot threat controls

Implemented October 9, 2026; **source-only, not built/deployed**. The running advisor-fix
image still has the old 50% AoE cutoff. These commands require an explicitly authorized
worldserver build/deployment before they become available.

Playerbots publication: `68556d789ee3420ff0eadaca11d5e9b03828f473`; root pin updated.

## Commands

Use inside the raid instance, with GM command permission (`SEC_GAMEMASTER` or above).
Console use and nonraid maps are rejected. GM *mode* does not need to be enabled.

```text
.raidthreat status
.raidthreat aoe 90
.raidthreat target 80
.raidthreat boss 70
.raidthreat healing emergency 30
.raidthreat healing normal
.raidthreat healing exempt
.raidthreat reset
```

Bare `.raidthreat` also displays status. Percentages must be integers 1–100; malformed,
partial, out-of-range and trailing arguments are rejected without changing settings.

Defaults:

| Setting | Default | Meaning |
| --- | --- | --- |
| AoE | 90% | Hold an AoE-threat action at or above this ratio |
| Current target | 80% | Subsequent hostile-current-target threat hold, including normal heals |
| Boss | 70% normally | Existing `AiPlayerbot.RaidThreatDiscipline.HoldPercent`, clamped 1–100 |
| Healing | emergency, below 30% HP | Bypass these threat holds for a genuine healing action on a living friendly recipient below this HP |

The emergency cutoff is **strictly below**: 29.9% qualifies, 30% does not. It bypasses
boss, AoE and current-target holds, not just the first AoE check. This is not a blanket
healing exemption. For earlier rescue attempts, `.raidthreat healing emergency 40`
raises the HP cutoff; it does not change threat production or enemy aggro mechanics.

- `healing normal`: use ordinary threat holds; disable the new emergency exception.
- `healing exempt`: explicit optional exemption for genuine healing actions regardless
  of recipient HP. Damage actions, including ones with friendly targets, remain checked.
- Existing Twins healing/friendly-support exceptions remain in **every** mode.
- Healing classification is `CastHealingSpellAction`, including shield actions derived
  from it. A spell does not qualify merely because its target is friendly; mixed
  damage/healing or other support spell classes are not automatically exempt.
- Recipient must be alive, in-world, on the same map and friendly to the caster.
  Mana, spell validity, range, LOS, cooldowns, target selection, other multipliers and
  encounter movement still apply. A bypass is not proof a cast will occur or land.
- Outside raids, healing retains normal threat behavior. The pending generic 90% AoE
  default still applies outside raids, as already authorized in the earlier change.

All three limits are AI safeguards, **not** the game's melee/ranged aggro-pull thresholds.
The ordinary threat values retain their existing cached uint8 calculation. AoE uses the
maximum evaluated enemy ratio, not an average; the denominator normally refers to the
highest-threat other recognized living tank, not necessarily Ari or the designated MT.
Changing a threshold does not correct or replace that calculation.

These are **generic ThreatMultiplier controls**. The global boss-discipline enable flag
is still respected and shown in status; this command does not turn it on/off. Existing
encounter-specific checks are untouched, including the separate Twins attack-action
check that reads the global boss hold configuration directly. Changing `.raidthreat boss`
is not a promise to retune every encounter-specific attack check.

## Scope, lifetime and diagnostics

Settings belong to `(map ID, instance ID)`, not the character, account or entire server.
A first edit snapshots the effective defaults; subsequent edits retain the other values.
Changes take effect on subsequent AI evaluations without another restart. In-flight
casts are not cancelled. **Tune between pulls**, rather than changing policy mid-cast.

Overrides and observations are in memory only. They disappear when that instance unloads
or the server restarts. `reset` clears observations and restores the current defaults;
it does not mean "disable threat control" or "disable emergency healing". No DB, gear,
roles, strategies, skills, encounter state, saved progress or lockout rows are written.

`status` shows effective limits, defaults, override state and boss-discipline enablement.
For each recorded healing bot it shows:

- latest evaluated heal action, recipient, recipient HP, reason and age;
- separately, the last blocked heal and its age, even if a subsequent heal check passed;
- evaluated AoE/current-target ratios and policy limits. Unevaluated later gates are
  explicitly labeled **not evaluated**, never zero threat.

Reasons distinguish boss ownership/threat hold, AoE limit, current-target limit,
emergency exemption, full healing exemption and retained early exits. Only the first
blocking gate is recorded. Status does **not** recompute threat, identify the secondary
enemy responsible for an AoE maximum, inspect all other multipliers, or report successful
casts. An old record does not prove a healer is still blocked. A candidate action check
is not necessarily the action the scheduler subsequently selected.

Observation storage is bounded to 80 recently checked bots per loaded raid; the oldest
latest-check record is evicted when needed. No actor pointers are retained. A mutex
protects concurrent map workers/commands. Policy generations reject in-flight old-policy
observations after a setting change or reset. A map-destruction hook clears instance data.

## Implementation and validation

Native playerbots files:

- `src/Ai/Base/Strategy/ThreatStrategy.cpp`: consume effective policy, healing admission,
  and first-gate observation; retain offensive guards, Twins exceptions and FocusMultiplier.
- `src/Ai/Base/Util/RaidThreatControl.h`: settings, exact command parser, synchronized
  per-instance store, bounded diagnostic history and generation protection.
- `src/Script/RaidThreatControl.cpp`: runtime adapter, GM command, map-destruction hook.
- `src/Script/PlayerbotCommandScript.cpp`: register the new scripts.

No core, encounter, threat-math, config-template, live-config, SQL or module-mirror edits.

**31 offline tests passed** in 83.365s, including:

- full production multiplier and healing threat classification, all 65,536 uint8
  AoE/current pairs across six action classes in normal mode, old-50% reproduction,
  threshold mutants, emergency boundary and all three bypassed gates;
- ordinary damage/friendly nonheal actions remain checked; invalid healing recipients
  cannot bypass; boss override reaches the existing helper; first-block diagnostics;
- production parser/store: invalid arguments, limits, map/instance isolation, modes,
  reset/unload, generation rejection, observation bounds, four-thread stress;
- complete production command/runtime/map-hook code with bounded core doubles:
  registration/security, console/invalid-map rejection, defaults, commands, status,
  current/last-block separation, numeric display and map destruction;
- retained pull authority, tank modes/acquisition, covered-target damage, Feelesia recovery
  and current Twins production helpers/actions/policies.

New fixtures use ASan/UBSan and warnings-as-errors. Native-header syntax checks pass for
all three changed/new translation units; scoped official C++ codestyle passes. These are
not a worldserver link/build, a live command test, or live Kael acceptance.

Twins historical patch fixture construction now uses its compatible pre-controls
`8d73b1a...` ThreatStrategy snapshot; its semantic executable still compiles **current**
production ThreatStrategy. The double gained friendly/engagement APIs and a default-policy
observation stub. The unrelated known pinned-replay test is not part of this test count.

Private evidence: `backups/raid-threat-controls-20261009-185711/` (pointer
`/tmp/raid-threat-controls-evidence`). Initial syntax compilation caught an unqualified
`Console` enum and it was fixed. Earlier Twins runs exposed stale patch-fixture/API
assumptions, corrected as described above. The first combined suite included a nonexistent
`test_tank_target_protection` module; that import error is retained, and the corrected
31-test command passed. Tank-target protection is covered within `test_tank_modes`.

Runtime inspection retained the advisor-fix container/image/start time, zero restarts
and no OOM. No server build, restart, configuration change or live gameplay command was
performed for this implementation.

See [the live healing investigation](tk-healing-threat-investigation-20261009.md) for
what was actually observed before Ari's earlier weapon-phase death. Those samples are
not evidence for the later bear attempt, nor proof that threat policy is the only remaining
healing problem.
