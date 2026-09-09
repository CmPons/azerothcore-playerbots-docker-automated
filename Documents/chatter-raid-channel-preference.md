# Ambient chatter prefers raid chat

## Status

**Built and deployed on 2026-09-09**, with raid preference 80, in the authorized
worldserver-only QoL release. See [deployment record](raid-qol-deployment-20260909.md).
No SQL changes or Pi bridge restart were needed. In-game routing observation
remains pending.

## Behavior

General, guild and group ambient contexts previously ran independent timers. A
raiding bot could get picked for General/guild first, spending its global ambient
cooldown there instead of speaking to its raid.

New default: **80% raid preference** on General/guild ambient opportunities when
the selected speaker belongs to a normal raid with an in-world real player.

- Prefer that bot's own raid context, using **only that context's recent chat,
  identity and human anchor**. Never forward a guild/General conversation into raid.
- If the raid context is not ready, skip the public opportunity instead of
  bypassing its timer, streak cooldown or no-self-reply rule. Reschedule the public
  opportunity normally, so there is no repeated roll every world tick.
- Only an admitted job consumes the normal global ambient budget and bot cooldown.
  Waiting for raid readiness does not consume an event seed or invoke Pi.
- Native raid ambient opportunities continue as before. The global limits,
  per-bot cooldown, streak limits, reactive-first queue and Pi bridge limits are
  unchanged. This does not deliberately raise the configured chatter volume.
- Solo bots, ordinary parties, bot-only raids and automatic battleground/battlefield
  groups retain their old channel selection. `AmbientGroup = 0` disables preference.
- This is based on **raid-group membership**, not physically standing in a raid
  instance, and it does not affect unrelated guildmates.
- Direct whisper, say and party/raid reply paths are untouched. General/guild
  conversations belong to the ambient layer and are subject to this preference.
  Event-triggered group reactions already choose party/raid and are not rerouted.
  Built-in playerbot announcements outside this LLM module are also unchanged.

The percentage is a **routing preference per eligible public opportunity**, not
an exact delivered-message ratio. Availability, timers, cooldowns, native group
turns and queue admission determine the observed mix. At 100, an eligible raider
will wait for its raid rather than take that General/guild turn; at 0, ambient
routing follows the previous behavior.

Before delivering generated ambient group text, verify that the bot and original
human anchor are still in the original group and that its party/raid type still
matches. Drop stale results rather than leaking them into a different group.
This delivery guard also applies to existing ambient group turns. Preference is
chosen **before generation**; already-generated public text is never relabeled
and sent into raid chat.

## Configuration

```ini
# Both local .env files; setup source
CHATTER_AMBIENT_RAID_PREFERENCE_CHANCE=80

# env/dist/etc/modules/mod_playerbot_chatter.conf
PlayerbotChatter.AmbientRaidPreferenceChance = 80
```

Accepted range 0–100; invalid config values fall back to 80 with a warning. The
compiled default and module dist template also use 80. `setup.sh` writes only the
new knob when its chatter config block is applied. Local env/runtime values are
prepared before deployment and activated by the authorized new worldserver start;
broad setup and config reload were not run.
Private pre-edit env/config backups are under `backups/chatter-raid-preference-*`;
`/tmp/chatter-raid-preference-backup` records the exact local path.

## Files and verification

Canonical `modules/mod-playerbot-chatter/` and its `azerothcore-wotlk/modules/`
build-tree copy are synchronized. Changed files: `PBChatterAmbient.cpp`,
`PBChatterWorld.cpp`, `PBChatterConfig.{h,cpp}`, new `PBChatterChannelPolicy.h`,
and the dist config. Reactive observer/queue and Pi bridge code were not changed.

```bash
python3 -m unittest discover -s scripts/tests -p test_chatter_raid_routing.py -v
bash -n setup.sh
```

Five tests include:

- Compiling the **production ambient director** with C++20 and warnings-as-errors,
  using minimal game API doubles and stubbed prompt/queue/event boundaries. This
  is not merely a reimplementation of the routing algorithm.
- All 100 rolls for both public channels; destination-only history and metadata;
  0/80/100 preference; unchanged non-raid/BG/bot-only cases; blocked/absent raid
  contexts; global/bot cooldowns; queue busy; source-anchor validation; correct
  destination anchor; no budget/event consumption while holding for raid readiness.
- Group identity guard cases and delivery source contracts. The full world script
  has not been compiled in this test harness or validated in-game.
- Isolated setup defaults, overrides, idempotence and preservation of other settings.
- Canonical/build-tree source equality.

The targeted official C++ style checker, shell syntax check and all thirteen Pi
bridge regression tests also passed.

The full worldserver build and authorized deployment subsequently passed. Real
raid/group membership changes during generation and observed in-game channel
distribution remain pending. No new SQL or Pi bridge restart was needed.
