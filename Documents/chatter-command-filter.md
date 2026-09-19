# Chatter command filtering

`PlayerbotChatter.CommandKeywords` suppresses AI replies to bot commands and keeps
those commands out of ambient conversation context. It matches the whole command
or a leading keyword followed by a space, case-insensitively—not arbitrary mentions
of the word in normal conversation.

## Pending change: `stats` — not deployed

At the user's request, bare `stats` has been added to the distributed config and
compiled fallback list. Both root and native build-tree module copies are synced.
`pvp stats` was already present but does not match a message containing only `stats`.

Examples after activation:

- `stats`, `  StAtS`, `stats all`: ignored by chatter; normal bot command handling remains.
- `what stats should I prioritize?`: still eligible for conversation.

**The runtime config remains unchanged while the user is playing.** No reload,
build, restart or bridge interruption was performed. This is staged alongside the
[pending friend-cap increase](roster-world-bots.md#pending-friend-protection-cap-increase--not-deployed).

For a future authorized activation, append `stats` to the existing live
`PlayerbotChatter.CommandKeywords` value without replacing other custom entries.
Updating the template or deploying a binary alone does not override that existing
explicit setting. No C++ rebuild is required for the live keyword setting itself.

Offline check: `python3 -m unittest scripts.tests.test_chatter_commands -v` compiles
the production classifier method with both actual shipped keyword lists.
