# Conditional old-raid resets

## Status

Implemented in source, **not built or deployed**. The running worldserver still
uses the previous global daily resets. Preparing these files does not protect a
live save from its next reset. No live binds, boss states or deadlines were edited.

User-selected policy:

| Save | Deadline |
| --- | --- |
| No successful boss completion yet | Next configured 04:00 daily reset |
| Unfinished, at least one boss completed | Exactly 72 hours from the first successful boss completion |
| Fully cleared | Next configured 04:00 daily reset after completion |

The three-day progression deadline is **not rounded to 04:00**. For example, a
first kill Monday at 21:00 expires Thursday at 21:00. Finishing Wednesday evening
switches it to Thursday's 04:00 reset instead. Finishing just before the progression
deadline can move expiry later, to the next 04:00. A clear exactly at the reset
boundary schedules the following day's reset, not an immediate reset.

The daily calculation matches the core's existing epoch-day arithmetic and
`Instance.ResetTimeHour = 4`. It does not use the host's timezone or DST; do not
assume that means 04:00 on a player's local clock. Different root/server `.env`
timezone values were noticed but deliberately left untouched.

## Scope and completion rules

Supported maps: Onyxia 249, ZG 309, MC 409, BWL 469, AQ20 509, AQ40 531.
Other raids, normal dungeons and heroics keep their existing policy and rate.
There is no speculative fallback for unrecognized instance serialization layouts.

Completion uses the instance script's persisted **DONE states**, not the
`completedEncounters` kill-credit mask. In particular, a premature phase-one
Razorgore death must not start progression or count as a completed encounter.

- Onyxia: one encounter.
- MC: all ten encounters, including Majordomo's successful defeat.
- BWL: all eight encounters.
- AQ20: all six encounters.
- AQ40: all nine encounters, including Bug Trio, Viscidus and Ouro; skip unused
  boss-state index zero.
- ZG: five priests, Hakkar, Mandokir and Jin'do. Fishing/Gahz'ranka and Edge of
  Madness are **not required** for a daily clear. Completing either summon event
  can start the three-day clock. Thekal's zealots and Ohgan do not start it.

Killing only the final boss is insufficient. The policy is independent of raid
size, scaling, human/bot attendance and other copies of the same raid.

## Persistence and lifecycle

New characters table: `instance_progression_reset`, keyed by instance ID, stores
phase, deadline and explicit-extension deadline. The core `instance.resettime`
remains zero for raids, so normal-dungeon startup cleanup cannot delete a retained
raid. Global raid warnings and resets skip managed copies, both in the save loop
and the loaded-map loop.

Boss-state saves and phase/deadline updates share a database transaction. Metadata
upserts reject backward phases and older same-phase deadlines: multiple async DB
workers must not undo a clear or an explicit extension with a stale checkpoint.
This does not redesign the core's unrelated instance-data write ordering.

Timers are restored before character binds are loaded; expired timers are processed
after binds load and before login. Normal restarts/logins, later boss kills, and
repeated saves never replenish time. Explicit player lockout extensions retain the
core's per-character semantics; consuming an extension and updating its deadline
share a transaction. If both deadlines passed offline, expiry catches up before
login rather than granting another fresh interval.

Live expiry uses the core's existing unbind/map-reset/respawn-cleanup path, scoped
to one ID. It defers while an encounter is in progress or any player in that copy
is in combat. The deadline stays overdue and the reset runs once idle. A stuck
encounter can consequently defer expiry until recovered; this does not fix the
separate Razorgore NPC-combat lockout investigation.

Instance-local deadline/stage fields are atomic for readers on other map workers.
The world-thread expiry pass runs after map workers join, and snapshots IDs before
teleports can invalidate the save container's iterators.

Personal raid-info/calendar lockout records and entry/relogin warnings use the
actual copy's deadline. First completion/clear sends a short human-only system
message. The general calendar's repeating *global raid schedule* still describes
the global cadence; the client has no single repeating schedule representing these
conditional timers. Use personal lockout information for your copy.

Administrative boss-state resets do not rewind an already-started timer or reopen
a cleared phase. Use a fresh copy for testing; these timers are deliberately not
refreshable by repeatedly resetting a boss.

## First activation and configuration

There is no historical first-kill timestamp to recover. On first activation:

- Existing partial saves get **one full 72-hour grace window** from activation.
- Existing completed saves get the next daily reset.
- Empty saves get the next daily reset.
- Unreadable legacy boss data gets the conservative progression window, never a
  guessed full-clear reset.

Adoption is persisted synchronously during startup. Existing valid metadata is
restored without refreshing its deadline. Disabling the feature stops adopting
new saves but **continues honoring existing managed saves** until they expire;
it does not abruptly discard their progress at the next global reset.

Core template/setup default: disabled. Prepared local values in both `.env` files:

```ini
RAID_PROGRESSION_RESET_ENABLE=1
RAID_PROGRESSION_RESET_DAYS=3
```

Prepared runtime `azerothcore-wotlk/env/dist/etc/worldserver.conf`:

```ini
Instance.ProgressionReset.Enable = 1
Instance.ProgressionReset.Days = 3
Instance.ResetTimeHour = 4
```

Configuration is read at startup, not dynamically reloaded. Days accepts 1–30,
with invalid values falling back to 3. Changing it doesn't rewrite existing
persisted deadlines. `Rate.InstanceResetTime` remains unchanged for other content.
Only the two new settings were prepared; broad `setup.sh` was **not** executed.
Private pre-edit copies: `backups/raid-progression-preparation-20260908-165937/`.

## Delivery and deployment requirements

Canonical patch: `patches/0019-core-progression-raid-resets.patch`.
Core policy: `src/server/game/Instances/ProgressionRaidReset.h`.
Migration: `data/sql/updates/pending_db_characters/rev_1788894000000000000.sql`.

**Every image built from this working core needs the new table before startup,
even with the feature disabled:** the prepared statements reference it. The SQL
is additive/idempotent and does not itself update existing saves or binds.

The worldserver Docker target does not ship core SQL or run database updates.
Deployment therefore requires separately approved migration application and
worldserver recreation, not just retagging an image. Before activation, back up
all databases, configs and the running image, with a final save snapshot after a
clean stop. Apply only the targeted migration, not a broad updater/setup run.
Do not rely on an unapproved activation to preserve tonight's BWL save.

Build only with explicit permission and host networking. A full working-tree image
also includes the pending persistent ten-player scaling change and any unrelated
local module edits. No full worldserver compile or in-game integration test has
been performed for this feature.

Rollback: reverting just the binary reinstates global resets and ignores the
metadata table, potentially shortening retained progression saves. Preserve the
backup and plan rollback timing explicitly; turning the new config off is the
safer gradual return to global resets. Do not casually drop the metadata table
while any binary that references it is running.

## Verification

```bash
python3 -m unittest discover -s scripts/tests -p test_progression_raid_reset.py -v
# Optional: same suite plus MySQL connection-local TEMPORARY tables only:
AC_TEST_MYSQL_TEMP=1 python3 -m unittest discover -s scripts/tests -p test_progression_raid_reset.py -v
```

Nine tests passed with the MySQL test enabled. Includes a standalone C++20 policy
test (`-Wall -Wextra -Werror`), boundary/legacy/completion cases, source integration
contracts, isolated idempotent setup, exact patch apply/reverse round trip, and
real prepared SQL tested against temporary tables that disappear on disconnect.
No persistent table was created by the SQL test. Targeted official C++/SQL style
checks, shell syntax, seven existing raid-scaling tests and three AH setup tests
also passed. Source contracts are not runtime
proof of map eviction, save restoration, extensions or encounter behavior.

Required authorized disposable-instance checks before calling this deployed/tested:

1. Partial BWL and cleared MC receive different correct deadlines; global 04:00
   resets neither warn nor erase the partial copy.
2. All required bosses, not just the final boss, switch the save to daily reset.
3. Restart before expiry preserves the exact deadline; restart after expiry removes
   only the expired copy while preserving unrelated binds/respawns.
4. Loaded/unloaded expiry, empty copies, player extensions, expired extensions,
   combat deferral, and complete-and-loot close to 04:00 behave as documented.
5. ZG summons/adds and AQ40's unused index do not block completion; failed Razorgore
   phase one does not count. Inspect native lockout/entry timers for humans and bots.
