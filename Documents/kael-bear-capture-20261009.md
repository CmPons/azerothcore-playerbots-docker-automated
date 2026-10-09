# Kael bear-off-tank capture — October 9, 2026

## Scope and intervention boundary

Separate from the earlier [full capture](kael-full-fight-capture-20261009.md) and the
user's intervening unrecorded attempt. The user requested recording after starting
this pull with Ailina as bear off-tank. Her captured combat strategies confirm `bear`
and `tank assist`; exact assist-tank indices were not queried.

Read-only capture: **20:59:24.100–21:06:48.495 CEST**, 37 rounds, **666 responses** across
nine bots, zero query errors. Stop requested 21:06:50; collector exited 21:06:51.561.
No recording remains active. Private evidence:
`backups/kael-bear-fight-20261009-205924/`, including raw `live-samples.jsonl`, collector,
`state-target-timeline.txt`, `late-fight-detail.txt` and `SHA256SUMS`.

The user explicitly disclosed **only `.damage` on Kael, after Redshift died**. No other
intervention was reported. The precise command timestamp and human death event were
not captured. Preserve the pre-intervention encounter evidence, but do not call the
ending an unassisted clear. Later bot resurrection actions are visible; do not invent
GM revivals to explain the recovery.

## Observed progression and phoenix

- All nine bots are sampled alive entering active Kael combat. Last-advisor targets
  transition to Kael around 21:04:55–21:05:07.
- First phoenix GUID in sampled potential-target lists: **21:05:48.020**, from Feelesia,
  creature entry 21362 / GUID 17379391320417633323.
- **21:05:51:** Ari selects Phoenix, bot HP 81% / phoenix HP 90%, approximately 0.44yd apart;
  phoenix-handling action reports OK.
- **21:05:52:** Ailina first sampled dead; her previous 21:05:40 sample was 94% HP on Kael.
  She is never sampled targeting a phoenix.
- **21:06:03:** Ari still selects Phoenix, bot HP 44% / phoenix HP 51%, approximately 1.50yd
  apart, and has two incoming attackers. Their identities are not established by that
  count.
- **21:06:15:** Ari first sampled dead. Feelesia was first sampled dead 21:06:00.

Only one distinct phoenix GUID and no egg were found in sampled potential-target lists;
there were no sampled egg targets. This is not proof that nothing could appear between
snapshots or outside per-bot visibility. The intended two-phoenix tank split was not
observed. A selected target, proximity and an OK action are not a full threat/victim log.
Do not claim the phoenix caused Ailina's or Ari's fatal damage.

## Both priests were compromised around the first spawn

The user observed healer mind control. Cross-bot hostile-target lists strongly support
its approximate timing, even though the endpoint did not record aura application events:

| Time, CEST | Evidence |
| --- | --- |
| 21:05:41.301 | Meliah's potential-target list still contains Kael, not the party. |
| 21:05:42.416 | Keilmere's list instead contains Redshift and multiple party members, not Kael. |
| 21:05:43.545 | Raney similarly lists the party as potential hostile targets. |
| 21:05:44.664 | Beliona lists both Meliah and Keilmere as potential hostile targets. |
| 21:05:53–54 | Both priests select Redshift; their lists contain party members rather than Kael. |
| 21:05:56.730 | Beliona lists Raney, Meliah and Keilmere together as potential hostile targets. |
| 21:06:08–11 | Cross-bot lists still contain controlled-party candidates, with Meliah dropping out before Keilmere in these non-atomic readings. |
| 21:06:17–18 | Meliah records Greater Heal OK; Keilmere records Resurrection OK. |

This identifies an approximate **21:05:42–44** loss of both priests, shortly before
first phoenix detection and Ailina's death. Raney is also strongly implicated in that
wave. Target-list caching prevents second-exact onset/removal claims. These observations
materially distinguish this attempt from the earlier uncontrolled-multiple-phoenix report:
losing both healers coincides with the first phoenix, not a demonstrated second-bird
assignment failure. Fatal-hit attribution remains unavailable.

## Installed mind-control mechanics and ten-player balance

At core revision `7ecb74c2a3c95bf1aa83e38a0e3ca197beb6cb72`,
[`boss_kaelthas.cpp`](https://github.com/CmPons/azerothcore-wotlk/blob/7ecb74c2a3c95bf1aa83e38a0e3ca197beb6cb72/src/server/scripts/Outland/TempestKeep/Eye/boss_kaelthas.cpp#L637-L680)
explicitly casts spell 36797 with **`SPELLVALUE_MAX_TARGETS, 3`**:

- First scheduled cast 20–23 seconds after active Kael begins; repeats 23–26 seconds.
  Cast scheduling can delay actual execution.
- The 50% random check controls his spoken line, **not whether he casts mind control**.
- Native target filtering excludes his **actual current victim**, nonplayers and targets
  outside line of sight. It does not reserve healers or honor the player's assigned MT
  label for exclusion. The generic spell engine randomly limits the filtered target list.
- The first phoenix is scheduled at 50 seconds, so the second mind-control wave is
  nominally around 43–49 seconds, immediately before it. This naturally makes simultaneous
  healing loss and phoenix arrival look like a phoenix-only problem.
- Target count is fixed, not multiplied by this workspace's ten-player HP/damage scaling.
  Three players are 12% of a 25-player raid but 30% of this ten-player roster. Both priests
  can be selected; there is no healer-preservation filter.

The intended counterplay is using **Infinity Blade** attacks to break mind control.
The bot implementation has a dedicated MC-breaking action gated on owning an Infinity Blade, but the capture is not proof that
weapons were properly equipped or that breaks succeeded. Balance and bot execution
must be considered separately.

A ten-player cap of one target would be a defensible proposed tuning starting point
(`3 × 10 / 25 = 1.2`), not a claim that it is Blizzard's native rule or that it alone fixes
the encounter. **No target-count, encounter, AI, config, SQL or service change was made.**
Any implementation and especially deployment require separate authorization.
