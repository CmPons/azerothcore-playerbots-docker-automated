# Solo healer offense and druid travel forms — staged, not deployed

Playerbots fork: `c806c14b0cc35e29c42402ef9f13d0ba3796a29a`.
The running September20 image still contains `a72a2ac6`. The user authorized source
work, not another build/restart. No live config, bot command, database write or service
interruption was performed. A future image build and authorized deployment are required.

## Two observed stalls

1. Ailina's saved restoration profile lacked `healer dps`. She selected a buzzard,
   entered combat AI mode and stopped progressing. The user's `co +healer dps` command
   immediately enabled attacks. Keilmere's saved priest profile had the same omission.
2. Later, with `healer dps` active, Ailina selected a Lagoon Eel while in Aquatic Form.
   The user confirmed she was underwater and in range. `do caster form` immediately
   enabled attacks. After killing the eel she resumed RPG wandering and seal form,
   then joined Eye of the Storm. Returning to seal after the kill was normal; failure
   to leave it for the next offensive spell was the bug.

The AI's combat mode is not proof that a creature has actually aggroed the bot.
Neither incident justified a pathfinding rewrite, teleport or character/profile reset.

## Changes

### Additive solo healer support after profile loading

The existing `RestoreWorldBotSoloStrategies` login hook now adds combat `healer dps`
when the **loaded strategy role is a healer**, and the map is not configured to restrict
healer DPS. It does not infer/replace the role from talents, respec, or replace any
saved combat strategy. It remains limited to the explicitly opted-in world-bot GUIDs.

Existing gates still exclude humans, grouped/mastered bots, instances/BGs, BG queues,
active Wintergrasp, and the protected stay/passive/runaway/move-from-group/combat-follow
orders. Ordinary NC follow is not a manual combat leash.
Non-healers, unselected bots and the other stored values are untouched.

This is an in-memory, login-time reconciliation after repository loading; it does not
write the profile or continually force DPS. A later manual `co -healer dps` remains
respected for that session. At the next eligible login the solo default is restored;
a saved omission alone cannot distinguish an intentional heal-only solo profile from
an old raid profile. Existing map restrictions and grouped healing behavior remain.

### Leave aquatic/travel form before healer damage

`DruidHealerDpsStrategy` now offers the already registered `cancel aquatic form` and
`cancel travel form` actions alongside its existing `cancel tree form`, at priority5.4,
ahead of Moonfire5.3 / Wrath5.2 / Starfire5.1. Each cancellation is useful only while
its exact aura is present, so it does not repeatedly consume ticks in caster form.

All three remain behind the existing `healer should attack` trigger. Group health,
mana and the Tree healing grace period are not bypassed; restoration healing priorities
still outrank offensive form changes. The new cancellations do not use `caster form`'s
unrelated mana threshold, which would otherwise strand a low-mana solo swimmer.

This correction applies wherever the druid healer-DPS strategy is enabled, including
BGs; it is not a name-specific exception. Aquatic Form's non-combat underwater trigger
is unchanged, so swimming resumes after a fight. Bear/cat/flight behavior and compatible
Moonkin Form are not changed. This is not a general rewrite of every shapeshift/spell
combination or of grouped healing while in a travel form.

## Regression evidence

```sh
python3 -m unittest scripts.tests.test_druid_healer_forms \
  scripts.tests.test_roster_world_bots scripts.tests.test_solo_battlegrounds \
  scripts.tests.test_companion_cap_setup scripts.tests.test_chatter_commands -v
```

All12 tests passed. Before editing production code, the new regressions failed on
exactly the missing `healer dps` restoration and missing aquatic cancellation assertions.
The C++ fixtures compile with C++20, warnings-as-errors and UBSan.

- Profile tests execute the actual repository loader, login repair, login hook and
  group/master transition code. They cover druid/priest healer roles, a non-healer's
  exact combat-profile preservation, map restrictions and their disable switch,
  manual orders, groups/masters, instances/BGs/queues, idempotence, and saved context.
- Form tests extract actual strategy/trigger/action bodies and cancellation class
  declarations/factory registration. They cover seal -> caster damage selection,
  kill -> swimming -> next target, land travel and Tree cancellation, low-mana solo
  cancellation, caster/Moonkin stability, unrelated forms, healing/mana gates, Tree
  grace and restoration healing priorities. Damage spells are not executed; action
  selection uses a priority/usefulness fixture, not the full engine or a linked server.
- Both modified native CPPs passed production-header `-fsyntax-only` checks. Scoped
  codestyle diagnostics matched the existing baseline; added-line width/whitespace
  and Git diff checks passed.
- Worldserver identity/image/start/restart count, Pi bridge identity, live configs and
  both private env hashes matched the pre-work snapshots.

Private validation logs: `backups/solo-healer-fix-20260920-105459/`.
No native image build/link or end-to-end water-combat test has been performed for this
revision. After an authorized deployment, verify repeated underwater pulls without
manual form commands, normal post-kill swimming, and retained party/raid healing.
