# Current social facts in chatter

This first slice adds current facts, not durable relationship memory. Existing personas,
reactive exchange history and the ambient conversation buffer are unchanged.

- Every reactive channel includes the speaker's guild and the sender's social facts,
  independently of the ten-member party/raid display limit.
- Ambient/event prompts include the speaker's guild and social facts for each displayed
  group member (at most ten). Ambient conversation replies also resolve the immediate
  sender by the buffered public-channel name. Join events include the current newcomer
  even when outside that display limit. Offline/unresolvable senders have unknown facts.
- Guild names come from GuildMgr. `same_guild_as_speaker` compares nonzero guild IDs,
  never names. Two guildless players are not guildmates. Self uses `not_applicable`.
  Missing guild names do not prevent equality being established from known IDs.
- `member_has_speaker_friended` and `speaker_has_member_friended` are separate current
  PlayerSocial lookups. One entry does not establish mutual friendship. Missing social
  state is `unknown`, not false; self is `not_applicable`.
- Names are quoted data, with control characters escaped, at most 96 input bytes
  without splitting a UTF-8 code point, plus `...` if truncated. No full friend list,
  private whisper text or extra chat history is collected by this feature.

Facts may inform natural familiarity; prompts explicitly avoid repeated guildmate
callouts, invented shared adventures, relationship strength, guild ranks or memories.

## Explicitly deferred: saved regular raid rosters

Saved regular raid-roster membership is **unavailable** in this slice, not false.
This scope reduction was explicitly approved during implementation: mod-raid-roster
only exposes synchronous DB reads, with no optional in-memory integration seam.
Chatter adds no raid-roster linkage, schema, queries or cache. Current grouping,
guild equality and friend entries must never be substituted for saved membership.
A future roster integration requires separate approval of the optional data seam.

## Validation and deployment

`python -m unittest scripts.tests.test_chatter_social scripts.tests.test_chatter_raid_routing -v`
runs offline tests using production social collection and ambient/event prompt code
with game-state doubles and the actual core StringFormat wrapper. The tests do not
link raid-roster or a database. They cover identity, missing/self state, one-way friends,
escaping/bounds, bounded members, immediate sender/newcomer coverage and existing routing.
An existing nonnull-unit TOML formatting bug was also corrected (escaped literal braces)
and is covered by an output regression.

The C++ change needs a normal worldserver rebuild and an explicitly authorized restart
to become live. Validation does not build an image, configure CMake or restart services.
