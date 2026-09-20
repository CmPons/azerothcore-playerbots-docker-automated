# Chatter command filtering

`PlayerbotChatter.CommandKeywords` suppresses AI replies to bot commands and keeps
those commands out of ambient conversation context. It matches the whole command
or a leading keyword followed by a space, case-insensitively—not arbitrary mentions
of the word in normal conversation.

## `stats` — active since September 20

At the user's request, bare `stats` has been added to the distributed config and
compiled fallback list. Both root and native build-tree module copies are synced.
`pvp stats` was already present but does not match a message containing only `stats`.

Matching behavior:

- `stats`, `  StAtS`, `stats all`: ignored by chatter; normal bot command handling remains.
- `what stats should I prioritize?`: still eligible for conversation.

During the [authorized September20 deployment](solo-companion-deployment-20260920.md),
`stats` was appended to the existing live `PlayerbotChatter.CommandKeywords` value,
preserving every other keyword. The worldserver restarted with that setting and the
updated binary. The Pi bridge was not interrupted. This was deployed alongside
[friend cap15](roster-world-bots.md#friend-protection-cap--15-active).

Updating the template or deploying a binary alone does not override an existing
explicit setting. Future keyword changes need an authorized runtime-config update;
no C++ rebuild is required for the live keyword setting itself. Matching is covered
by offline production-classifier tests; no in-game chat request was injected during
deployment.

Offline check: `python3 -m unittest scripts.tests.test_chatter_commands -v` compiles
the production classifier method with both actual shipped keyword lists.
