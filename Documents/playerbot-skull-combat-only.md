# Skull is a combat focus marker

Status: source prepared, not built/deployed. No server or bridge restart.

Skull previously inserted an otherwise idle mob into the attackers list and could
also be selected directly by the DPS/RTI selectors. It now only supplies automatic
attack targeting and priority when the bot's actual `IsInCombat()` flag is set.
The AI engine state or another group member's combat flag does not substitute.

- Real group attackers and intentionally prioritized targets remain eligible:
  bots can still assist a real pull before acquiring their own combat flag.
- Normal DPS, AoE and tank selectors share the RTI gate; other icons and CC
  assignments are unchanged.
- The explicit `attack rti target` action also checks skull at usefulness and
  execution, before its direct-icon fallback or any priority/pull-target mutation.
  To deliberately initiate a pull, use the existing normal attack/pull commands;
  `pull rti` retains its independent icon lookup.
- The attackers value keeps its normal one-second cache but invalidates it when
  the real combat flag changes, preventing a cached skull-only attacker from
  surviving the transition out of combat. RTI/DPS targets normally recalculate on
  each access.
- This is not a blanket passive mode and does not prevent unrelated grind,
  encounter, explicit command, or real group-assistance attack behavior.

Reproducible patch: `patches/0020-playerbot-skull-combat-only.patch`, applied by
existing setup/update patch handling to the nested playerbots source. The five
changed files had no preexisting edits; unrelated nested work is preserved.

Checks (3 passed):

```bash
python3 -m unittest discover -s scripts/tests -p test_skull_combat_only.py -v
```

The runner compiles extracted, unchanged production method bodies with game API
and cache doubles, exercises precombat/in-combat/postcombat targeting, stale RTI
fallbacks, cache transitions, explicit pulls, ordinary assistance, other icons and
existing validity checks. It also verifies selector/cache source contracts and
patch reverse/apply round-trip. This is not a full worldserver build or an in-game
regression test. A build and deployment require fresh approval.
