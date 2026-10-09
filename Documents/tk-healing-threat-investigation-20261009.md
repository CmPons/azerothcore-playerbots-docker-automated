# TK healing suppression and narrow AoE threshold adjustment — October 9

Investigation status: **one live blocker confirmed**. The original narrow threshold
adjustment was source-only at this checkpoint; no live strategies, configuration,
character data or services were changed during the investigation. The advisor HP fix
was deployed independently. See the later deployment follow-up immediately below.

**Later October 9 follow-up:** the user authorized [per-instance threat controls and
emergency healing](raid-threat-controls.md). That implementation supersedes the one-line
source-only proposal below, retaining its 90% default and adding tunable limits plus an
emergency healing path. It was [deployed at19:18:43 CEST](raid-threat-deployment-20261009.md)
after separate authorization. The investigation and narrow-change validation below remain
a historical record, not the complete current diff.

## What was captured

The user reported healers alive, in range and with mana, sometimes wanding instead
of healing Arinerica. Read-only command-server sampling during a subsequent weapon
phase captured explicit `Multiplier threat made action … useless` messages:

| Local time (CEST) | Bot | Selected healing recipient | Recipient HP | Bot mana | Explicit veto |
|---|---|---|---:|---:|---|
| 17:36:49 | Ailina | Arinerica | 14% | 92% | Rejuvenation |
| 17:37:21 | Meliah | Arinerica | 20% | 81% | Greater Heal, Heal, Lesser Heal |
| 17:37:23 | Keilmere | Arinerica | 14% | 82% | Greater Heal, Heal, Lesser Heal |

Ailina's AoE/current-target threat values were 136/9; Meliah's 80/80; Keilmere's
73/73. These are integer percentages reported by cached AI values, not raw threat
amounts. Basic/action and value queries were consecutive, not an atomic snapshot.
Nearby sampled positions put the priests about seven yards from Ari in the latter
examples. This establishes a threat veto independently of whether some other spell
attempts also failed range, LOS, movement, cooldown or cast-state checks.

The initial single-line reader was unsuitable for `values`: uint8 fields are
formatted as raw characters and can contain newlines. Its first mixed response
file is retained as **framing-invalid**, not used for these findings. The corrected
collector read each values response on its own connection until idle, storing raw
control characters as JSON escapes. Invalid UTF-8 bytes were backslash-escaped
(e.g. `\\x88` means 136). It completed 60 bounded rounds / 480 responses and stopped;
no ongoing instrumentation or debug configuration was installed.

Private evidence: `backups/tk-healing-investigation-20261009-173527/` (actual path
also recorded in `/tmp/tk-healing-investigation`). `live-samples.jsonl` contains the
corrected captures; `/tmp/capture-tk-healers.py` is the read-only collector. Initial
plain basic queries also caught both priests shooting at Capernian, but Ari was
already full health in that first sample. Those observations alone did not prove
why they had failed to heal earlier.

Pilbok/Kaaren were separately observed non-combat, targetless, repeatedly reporting
weapon-priority action failure at 17:36:34. A later check found them dead. No exact
failure return branch or persistent targeting deadlock was established; this
threshold adjustment is **not** presented as their targeting fix.

## Source explanation

`src/Ai/Base/Strategy/ThreatStrategy.cpp::ThreatMultiplier::GetValue` applies these
checks to actions that generate threat:

1. Existing taunt-immune raid-boss damage-hold policy (configured default 70%).
2. For AoE-threat actions, the highest calculated threat percentage across evaluated
   attackers: previously **50%**, now source-prepared as **90%**.
3. Current-target threat: **80%**, unchanged.

`CastHealingSpellAction::getThreatType()` returns `Aoe`. Consequently the generic
AoE check suppresses healing even for a critically injured selected party member.
`ThreatValue::Calculate("aoe")` takes the maximum across attackers, not an average;
its ordinary per-enemy ratio compares the bot with the highest threat among living
other group members recognized as tanks. One secondary enemy can therefore hold up
healing while the current enemy looks secure. There is also an opening zero-threat
special case that returns 100 with tanks present.

The priest healing strategy's fallback is `shoot`; its single-target threat type
can pass when the AoE check rejects heals. This does not mean every wand event was
caused by this rule, nor that Ailina literally uses a wand. The captured druid
failure was an explicit healing veto.

There is already a Twins-only exception for healing/friendly spell actions, but it
does not apply to TK. No dying-tank override exists in the generic checks. Changing
threat across enemies explains why this blocker can come and go. Kael's single-
advisor DPS-wait multiplier is a separate mechanism; the explicit vetoes above
came from `threat` during weapons, not that advisor wait timer. Applicability to
revived advisors follows the generic code; these particular live samples were
not a four-advisor-phase capture.

The old 50/80 pair is present in historical playerbot source `dfbbbf84`, before our
local raid-discipline/Twins work. No precise rationale for choosing 50 was established;
it is a hard-coded margin, **not the game's actual aggro-pull threshold**. Neither
this nor unchanged historical code proves which change introduced the user's
reported intermittent behavior. Additional Kael movement vetoes and unexplained
`IMPOSSIBLE` actions remain distinct observations.

## User-requested simple change and its limits

Only the generic AoE check changes **50 → 90**, plus an explanatory comment. It
applies to **AoE damage as well as healing**, wherever the generic threat strategy
is active. No healing exemption, emergency override, threat calculation, movement
policy or damage-hold setting changes. It is not a TK-only adjustment.

At exactly 90% the AoE veto still applies. The current-target 80% gate and existing
boss hold remain, so this is **90% tolerance on secondary enemies, not a universal
90% effective healing threshold**. Replaying the captured inputs through the
production multiplier with a nonholding boss-helper double shows:

- Keilmere 73/73: the old veto lifts.
- Meliah 80/80: still held by the current-target check.
- Ailina 136/9: still held by the new AoE threshold; a single-target action can pass.

Thus this is deliberately a limited first step, **not a complete healing fix**.
A later healing/friendly-support exemption would need separate scope and validation
while preserving actual damage discipline. No promise of live encounter success
is inferred from offline tests.

## Validation/publication

`scripts/tests/test_aoe_threat_threshold.py` compiles the complete production
multiplier and production healing threat-type method against bounded context,
config and encounter-helper doubles, with ASan/UBSan and warnings-as-errors.

Playerbots source commit: **`e77f18073641a2650c548832714be7198265e64c`**.

Six tests cover all 65,536 uint8 AoE/current-target pairs across healing, AoE damage,
single spells, attacks, pet attacks and neutral actions; remaining captured-input
limitations; null/group/neglect guards; existing boss/Twins branches; the old 50%
behavior; boundary/current-target mutants; and exact one-threshold source scope.
An initial test-fixture failure was caused by a mutant binary named `current`
overwriting the current-code binary. Unique mutant filenames fixed the harness;
production logic was not altered to accommodate the failure.

**17 tests passed** with the existing raid pull-authority and Feelesia recovery
suites. Native-header syntax and scoped official C++ style checks passed. No image
build or deployment was performed. Native code belongs in the playerbots fork;
root publication includes its updated pin, tests and this report. Historical patch
replay is not used to publish or deploy this change.
