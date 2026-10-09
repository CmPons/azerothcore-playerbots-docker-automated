# TK advisor health deployment — October 9, 2026

The user explicitly authorized building/deploying the advisor health fix and stayed
offline for the operation. This does not authorize future service interruptions.

## Shipped scope

Root source commit **`322683b`**: [advisor health scaling](tk-advisor-health-scaling.md).
Only `modules/mod-raid-scaling/src/RaidScalingMgr.cpp` changed relative to the prior
running image. Both module copies match. Core remains
`7ecb74c2a3c95bf1aa83e38a0e3ca197beb6cb72`; playerbots remains
`8d73b1a5721848071cd5e3048c7ad84a8f1c8194`. No native pin changes were needed.

The four TK advisors now use the aura-aware unit-mod health base. Native
resurrection remains +100% HP, including during manual scaling reapplication.
At 0.4 scaling, expected revived maxima are 152,974 for Thaladred/Sanguinar/Telonicus
and 107,070 for Capernian. This preserves the buff rather than the old workaround's
lower first-phase maximum. Damage, encounter scripts/timers, bots, gear policy,
configuration and SQL were not changed. The reset-deadline deletion bug was not fixed.

## Validation and runtime

- Fresh four-database backups before building and after clean stop; gzip and
  checksums verified. No humans were online at preflight or immediately before stop.
- **82 tests passed**, including the new advisor suite, Twins, respawn, creature
  eligibility, scaling defaults/support, Feelesia recovery, dead-bot loot,
  Leotheras reset and direct token loot. Native-header syntax check passed.
- Image build: **16:53:52–16:54:39 CEST**. Sixteen warnings were checked against the
  previous source manifest; all originate in unchanged files.
- Never-started candidate inspection verified binary dependencies, retained fixes,
  and both advisor apply/restore paths' map/entry scope and unit-mod base update.
- **4,208 source records and 48 config/policy records** stayed unchanged throughout
  the operation. Module mirrors and database updater ledgers matched.
- Only worldserver was cleanly recreated, with `--no-deps --no-build --pull never`.
  Other containers and the Pi bridge were not restarted.
- Worldserver **ready at 16:56:03 CEST / 14:56:03 UTC**.

```text
Image: acore/ac-wotlk-worldserver:tk-advisor-20261009-164927
Image ID: sha256:bf4efa82c0597115c008df6e5d0f01a14a9730386f55802ce0ee551c6211ee97
Binary SHA256: 63bc89aae52029478d35547c0ddad5aa31c5f7297a7a66225f5d0b0f9ce08df7
Container: 043b2810937bb61d248027093dc4c8dea5e727f9e3ce935bbcb834121f8f52da
Started: 2026-10-09T14:55:43.599596488Z
Rollback: acore/ac-wotlk-worldserver:pre-tk-advisor-20261009-164927
```

## Preservation

The prebuild-to-stop comparison, still on the old image, retained all 2,112 item
identities/positions and 717 equipped identities. Kaaren's temporary weapon enchant
aged, Raney's Mana Emerald used one charge, and his Water Elemental's saved health,
mana and timestamp changed. These are distinguished from new-image startup changes.

Immediate startup retained all **2,112 items, every captured item field and position,
and all 717 equipped identities** exactly. All 19 pets and their captured fields,
spells and action bars matched. No group existed to disband. Raid progress, all ten
permanent TK binds and the previously restored deadline row matched exactly:

```text
TK 6510: completedEncounters=7; data="T E 3 3 3 0 "
instanceId stage resetTime  extendedResetTime
6510       2     1791648306 1791907506
```

The ordinary deadline remains **October 10, 18:05:06 CEST**. No restoration SQL was
performed during this deployment.

One startup difference remains explicitly recorded: Raney's First Aid (129),
Cooking (185) and Fishing (356) **maximum skill values changed from 350 to 450**.
Their earned/current values stayed 350; no spells or talents changed. This is not
lost skill progress, but its exact startup normalization path was not traced and
strict abilities preservation did not pass. No skill edits/restoration or audit
allowlist was applied.

After the **380-second observation** (beyond the five-minute player save interval),
worldserver remained healthy with zero restarts/OOM. Other services, source/config
manifests and updater ledgers matched. TK progress, deadline and all ten binds still
matched. All **717 equipped identities**, existing gems and permanent enchants were
retained; no existing inventory item moved.

The post-save comparison also recorded these differences:

- Beliona's Soul Shard and Master Soulstone were replaced by new item GUIDs of the
  same entries/counts. No equipment disappeared. Total item identities went from
  2,112 to 2,113 because Feelesia also acquired Zeppit's Crystal (31815) alongside a
  new `Bloody Imp-ossible!` (10924) quest-log row.
- Feelesia used eleven Timeless Arrows (23→12) and one Adamantite Sharpening Stone
  (14→13); Netherbane gained temporary enchant 2713. Kaaren's temporary enchant aged.
  These are consistent with normal autonomous activity; no assistant item commands
  were sent, and no gems/permanent enchants changed.
- All 19 pet identities/levels/spells remained. Saved HP/resources/happiness/times
  changed for Beliona's demon, Raney's elemental and Feelesia's cat. Pet action bars
  matched, but Waterbolt (72898), Growl (27047), Claw (27049) and Rake (59885) autocast
  flags changed **129→193 (enabled)**. Their precise action path was not captured;
  these toggles are not attributed to advisor health scaling or silently waived.
- Raney's three skill-cap changes persisted; earned values remained 350. No other
  spell/talent changes, profile loss or group/bind loss were recorded.

Infrastructure verification passed (`observation.exit=0`); strict preservation
remains separately recorded as failed (`preservation-observation.exit=1`, nine
unreviewed categories/field records). No blanket exception or restoration was
applied. This is a successful deployment with disclosed gameplay-state differences,
not an assertion that every database field stayed identical.

## Evidence and acceptance boundary

Private, ignored evidence: `backups/tk-advisor-deploy-20261009-164927/`, pointer
`/tmp/tk-advisor-deploy-backup`. It contains both full dumps, source/config snapshots,
tests, syntax/build/binary checks, raw comparison failures and preservation review.
These artifacts and live configs/credentials are not in Git.

Source tests and binary inspection establish the shipped implementation, not a live
Kael victory. Ordinary encounter acceptance remains with the user's next attempt;
weapon-phase healing and other deferred issues are not claimed fixed.
