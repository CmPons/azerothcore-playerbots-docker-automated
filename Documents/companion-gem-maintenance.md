# Companion gem-only maintenance

## Status

Implemented and offline-tested; **not built into or enabled on the running server**.
No live configuration, gear, database rows, services or containers were changed.
Enabling this initially requires an authorized worldserver build/deployment and the
explicit opt-in below. Do not run full `setup.sh` or restart anything just to try it.

## Policy

- Applies to online **bots on a human-account character's friend list** and **bots in
  any saved raid roster**. Random/addclass account owners do not supply friend-list
  eligibility. Human players (including AI-controlled humans) and unrelated filler
  bots are excluded. Offline roster members catch up when they next log in.
- Friend eligibility is independent of the persistent-companion protection cap;
  this does **not** change that cap or weaken the separate gear-generation safeguards.
- Only fills **empty sockets on equipped gear**. Receiving socketed loot or equipping
  an upgrade requests an early check; bag-only gear remains untouched until equipped.
- Provision is **free and direct**, not AH shopping, crafting, or consumption of bag gems.
  No loose gem item is created, so full bags do not block it.
- BC rare/blue baseline. A deterministic hash of bot GUID, item-instance GUID and socket
  index allows an epic for 20% of normal sockets. Existing gems never reroll. Blue
  fallbacks remain available when better suited to the role or needed for meta activation;
  consequently actual epic share can be below 20%. BC meta gems are blue.
- Uses `StatsWeightCalculator::CalculateEnchant`, which derives weights from the bot's
  loaded role, class and talent specialization. Respeccing changes choices for future
  empty sockets, **not** previously installed gems.
- Hybrid gems count toward both colors. Planning uses the actual DBC meta requirements,
  attempts to activate existing inactive metas, and never sacrifices an already active
  manual meta. An impossible existing inactive meta stays untouched; ordinary sockets
  may still be filled without disabling active metas. New metas are installed only when
  their conditions can be met. Conflicting constraints can leave sockets empty.
- A modest 20% score preference favors socket-color matching. Actual native socket bonuses
  and meta effects are recalculated during application. This is useful role-aware gemming,
  **not** a globally optimal/BiS loadout, exact socket-bonus-value or future stat-cap optimizer.
- No existing gems, permanent enchants, gear items, talents, professions, strategies,
  saved roles, money, group membership or instance binds are replaced/reset by this feature.
  Broad playerbot `maintenance`, factory initialization and `.raidroster sync` are not called.

## Scheduling and safety

Player hooks only request work on loot/equip; they never mutate the item inside the
inventory operation. A player-local timer performs a check about two seconds after
initial eligibility/login or a new-item/equip request. Periodic checks default to five
minutes plus a stable 0–30 second per-bot stagger. Repeated passes are idempotent.

Work waits until the bot is alive, in world, not removing/teleporting/logging out,
out of both actual combat and the AI combat state, not casting and not trading.
Broken, foreign-owned, unequipped, traded, refundable and soulbound-tradeable items
are not modified. Their protections are not cleared to make socketing possible.

Membership/catalog snapshots refresh on the world thread every 60 seconds (and after
configuration reload); map-thread player updates never query SQL. Friend/roster changes
can therefore take up to 60 seconds to affect eligibility. Catalog data and membership
are published together as immutable atomic snapshots. No cross-tick raw item/player
pointers or per-player SQL queries are retained. Logout destroys scheduling state with
the player.

The curated catalog has 95 rare/epic BC gem IDs, matching the relevant portion of
`config/ahbot/tbc-cut-gems.tsv`. Runtime validation excludes bound, unique-equipped,
limit-category, temporary and profession-restricted gems, and checks the bot's level
and item-use eligibility. There are no green, uncut or Northrend gems. This remains
BC-only if the realm's level cap later changes.

The planner is bounded to 54 empty normal sockets and 16,384 color states; unsupported
DBC rules or an exhausted bound fail closed. Standard BC gear has one meta socket;
nonstandard outfits with multiple empty metas handle one per pass and recheck all
constraints next time. Unfillable sockets are retried on a later pass; no placeholder
gem is forced into them.

Application mirrors `WorldSession::HandleSocketOpcode`'s socket-enchant removal/application,
socket bonus and meta-toggle ordering, while rejecting occupied sockets and trade/refund
protected gear. `Item::SetEnchantment` marks the item changed for normal character saving;
no direct gameplay SQL or forced save is used. Only the socket enchantment fields and,
where warranted, the socket bonus change. Success is logged as `[CompanionGems]` with
bot name, filled count and epic count.

## Configuration

Reproducible `.env` knobs (defaults shown):

```ini
COMPANION_SOCKET_GEMS_ENABLE=0
COMPANION_SOCKET_GEMS_INTERVAL=300
COMPANION_SOCKET_GEMS_MIN_LEVEL=61
COMPANION_SOCKET_GEMS_EPIC_PERCENT=20
```

`setup.sh` maps these into `mod_raid_roster.conf` without calling maintenance or changing
playerbot settings. The new feature is independent of `RaidRoster.Enable`:

```ini
CompanionMaintenance.SocketGems.Enable = 0
CompanionMaintenance.SocketGems.IntervalSeconds = 300
CompanionMaintenance.SocketGems.MinLevel = 61
CompanionMaintenance.SocketGems.EpicPercent = 20
```

Set Enable to 1 only as part of an explicitly authorized activation. Interval is clamped
to 30–3600 seconds, minimum level to 1–80, epic percent to at most 100. Disable stops future
passes; it does not remove gems already provided. Existing runtime/private `.env` files
were deliberately left unchanged during implementation.

## Source and validation

Root-owned `modules/mod-raid-roster/src/`:

- `CompanionGemMaintenance.cpp`: eligibility, immutable snapshots, scheduling, native
  integration and socket application.
- `CompanionGemPlanner.h`: dependency-free bounded color/score planner and stable quality hash.
- `CompanionGemCatalog.h`: curated gem IDs; parity checked against the AH catalog.
- `RaidRosterLoader.cpp`: registers the new scripts.

Native mirrors are kept byte-identical. Core/playerbots fork revisions are unchanged.

```sh
python -m unittest scripts.tests.test_companion_gems -v
bash -n setup.sh
git diff --check
python scripts/tests/raid_combat_syntax.py --output /tmp/companion-gem-syntax \
  modules/mod-raid-roster/src/CompanionGemMaintenance.cpp \
  modules/mod-raid-roster/src/RaidRosterLoader.cpp
```

The C++ planner and full production maintenance translation unit run under a fixture
with undefined-behavior sanitization and warnings-as-errors. Coverage includes membership,
human/filler exclusion, catalog restrictions, level gates, unsafe-state deferral, login/
loot/equip/periodic scheduling, preservation/idempotence, role changes, prismatic legality,
active/inactive manual metas, epic blue-fallback behavior and disabled/no-map-SQL operation.
Planner results are also compared with exhaustive enumeration for small instances.
An isolated setup test executes only the new config block with a stubbed `set_conf`.

Production-header syntax checks validate real native API signatures; fixtures do **not**
prove live stat application, persistence, concurrency, or complete native linkage.
The running server's DBC files were inspected read-only: all 95 catalog enchantments have
required level zero and no profession restriction; meta constraints include two-of-each,
relative-color and minimum-count rules. No binary client data is published.

For authorized deployment, capture fresh saves/config/source evidence first. Then verify
only expected empty socket fields and socket bonuses change, manual gems/enchants and
item identities remain intact, a newly equipped upgrade catches up, combat/trade defer,
metas activate, and generated gems persist through normal saving/relogin. Never restore
an old gameplay snapshot over newer progress simply to undo generated gems.
