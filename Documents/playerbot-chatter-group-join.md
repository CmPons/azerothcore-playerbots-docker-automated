# Party / raid join chatter

`PBChatterGroupEventScript::OnAddMember` adds `EventKind::GroupJoin` to the existing
chatter event queue. It reacts to successful membership changes, not invitations,
raid conversion, or ordinary login into an already-saved group.

## Behavior

- Both real players and playerbots can be the newcomer.
- At least one online real player must be in the group; bot-only groups generate no requests.
- Initial group creation's leader addition and automatic BG/BF groups are ignored.
- One eligible living bot speaks, using the existing event prompt/persona pipeline.
- Existing members can welcome the newcomer; a newcomer chosen as speaker greets the
  existing group instead of welcoming itself.
- Prompts must not assume prior friendship, a return visit, or physical arrival nearby.
- Member/group membership is rechecked before submitting the LLM job; leave/disconnect
  before processing drops the reaction. Existing queue safeguards handle the delivery context.
- Join bursts share a group-scoped 30-second dedupe key. Only the first candidate is
  queued during that window; suppressed joins are not postponed for a greeting backlog.
- Existing event group/bot cooldowns, per-minute ceiling, and bridge limits still apply.

Config (default; existing runtime configs need no edit to pick up the compiled default):

```ini
PlayerbotChatter.EventChance.GroupJoin = 80
```

0 disables direct join reactions, 100 always passes the chance check (other limits still apply).
Join facts also enter recent group context, like other event types.

## Validation

- Root source/config templates and AzerothCore build-tree copies synchronized.
- `git diff --check` passed; full host-network worldserver image build passed.
- Image: `acore/ac-wotlk-worldserver:chatter-group-join-test`.
- No restart, deployment, live party mutation, or generated-model request during development.

Deployed on 2026-09-06 with the approved BWL suppression update:

- Image: `acore/ac-wotlk-worldserver:bwl-suppression-fix-test` (also live `master`).
- SHA: `1f8289b93dbe39fd83d13a924113d9dfd68c35934198c0c70048c54c4707d4dd`.
- Worldserver start: `2026-09-06T19:44:01.75751003Z`; ready, restart count 0.
- Startup loaded 107 persistent personas. No live join test or deliberate model
  request was performed during deployment; the checks below remain pending.

Check in-game:

1. Human joins a party with a bot: a bot may briefly welcome the human in party chat.
2. Bot joins a human's party: the bot can greet existing members in first person.
3. Join a raid: response uses raid rather than party chat.
4. Fill a roster rapidly: at most one join candidate in 30 seconds, not one per bot.
5. Bot-only group, initial leader creation, BG auto-group: no join reaction.
6. Invite without accepting, raid conversion, normal relog: no join reaction.
7. New member leaves before event processing: no direct greeting.
8. Existing event cooldown / exhausted request budget: no bypass for joins.

These in-game checks remain pending; build validation is not a live behavior test.
