# Solo companion battlegrounds — staged, not deployed

The user observed Kaaren earning XP in Arathi Basin and requested BG participation
when the bots are not in a party or raid. This supersedes the earlier intent to use
the friend cap to exclude them from all automatic PvP matchmaking.

Playerbots fork revision: `a72a2ac66c2d438daaef5e48fd50751eb63cd81f`.
The running image still contains `f63a0463`; **no build, config reload or restart was
performed for this change**. Fresh authorization is required for deployment.

## Rule

Applies to every opted-in `WorldBotGuids` character, even beyond the friend cap,
plus existing friend-protected companions (including Ari/Meliah).

- Allow ordinary BG queueing while genuinely solo and unmastered, with no pending
  group invitation and no stay/passive/runaway/move-from-group or combat-follow order.
- Block automatic queueing in **any party or raid**, including when the bot is leader.
- Preserve the global BG enable switch and normal level/bracket, deserter, combat,
  login-delay and queue-demand checks. Do not force a particular match or alter XP rates.
- Reject arena team recruitment/queueing for these companions; this is a BG leveling
  option, not permission for arena captains to gather/disband their groups.
- Ordinary filler bots and human-controlled characters retain their existing rules.
- The original saved-profile correction now also restores `bg` alongside solo questing
  and grinding when BGs are configured on. It does not continually reassert strategies
  against commands issued later in the session.

## A queued bot must not abandon its new party

Eligibility is checked during action selection, execution, queue submission and at
actual core packet consumption using existing PlayerScript hooks. If a queued bot
receives/accepts a group invitation or gains a master, both normal match invitations
and the WAIT_QUEUE force-join recovery path send a leave-queue packet instead of
porting it. Cancellation does not disband/remove its group or reset combat strategies.
The final port hook also rejects a stale entry packet if party state changed after
that packet was queued. Leaving a queue/BG is never vetoed by this policy.

Bots already inside a BG are not forcibly extracted from its automatic raid. This
change guards new queueing/entry, not immediate recall out of a match already underway.
Explicit bot BG-entry actions are subject to the same safety guards; human clients'
manual queueing is unchanged.

## Other staged changes

The cap increase to **9** remains staged for other friend protections, including
scheduler teleports, LFG filler and existing maintenance behavior. With this native
change it no longer means a blanket ban on solo BG queues. Neither the existing
five-friend cap nor runtime configs were changed during this work.

The [`stats` chatter filter](chatter-command-filter.md) is also still staged. Future
activation must include the intended runtime settings; editing env/templates alone
does not change the existing live config. Do not run full setup or deploy while the
user is playing. No level synchronization, character rerolls, roster edits or
persistent quest/inventory writes are part of this source change.

## Validation and limits

```sh
python3 -m unittest scripts.tests.test_solo_battlegrounds scripts.tests.test_roster_world_bots -v
```

Six focused tests passed. C++20/Werror/UBSan fixtures execute the production companion
policy, packet-boundary hooks and cancellation branch with API doubles. They cover
all nine characters, world bots beyond the friend cap, cap9 compatibility, party/raid
leaders and members, pending invitations, masters, manual orders, disabled BGs,
instances/Wintergrasp, arenas, leaving queues and preserving an active BG's auto-raid.
Wiring checks cover both invitation paths, the legacy accept action, arena drafting
and registered core hooks. Saved-profile tests verify `bg` restoration follows config.

All four changed native translation units passed production-header syntax checks.
Native diff checks passed; focused codestyle findings were identical to the existing
baseline. No image build/link or real queue/group-transition test has been performed.
Live acceptance must verify solo participation, recruitment while queued, staying
with the party when a match becomes available, and normal behavior after leaving it.
This does not guarantee a particular XP/hour or time to level70.
