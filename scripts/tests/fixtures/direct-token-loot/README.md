# Installed tier-token scoring fixtures

`t4-items.json` is a read-only 2026-10-01 snapshot of public game-item data:

- 15 token items (29753–29767), 85 T4 rewards and 85 Gladiator alternatives;
- 170 distinct item/extended-cost paths from installed `npc_vendor` data;
- installed `item_template` class masks, levels, armor/block, nonzero stats,
  equip-spell IDs/triggers, socket colors/bonuses and set IDs;
- referenced extended-cost honor/arena requirements, spell effect fields and
  socket-enchantment stat effects, decoded using the core's DBC structures.

No character records, credentials, dumps or raw client DBC files are included.
Tests use this frozen JSON without contacting the server.

The 51 referenced **PvE** equip spells have generic spell family 0 and flat effects.
Their IDs have no matches in the current core's `SpellInfoCorrections.cpp` and no
installed `spell_dbc` overrides. The installed `itemextendedcost_dbc` table is empty.
The fixture executes production flat-aura/stat collection; spell lookup/positive
permanent-effect handling and socket enchant lookup are test doubles. Unknown
non-flat PvE spell types fail the fixture rather than silently scoring as zero.

The ten caster/healer score crossovers are deliberate test expectations for the
existing weights, not set mappings used by the implementation. See
`Documents/spec-aware-direct-token-loot.md` for the exact winners and limitations.

If regenerating, resolve T4 tokens through extended-cost item requirements and
`npc_vendor`, rather than assuming an item-ID range captures every set. Include
PvP alternatives so their exclusion remains independently testable. Recheck spell
corrections/DB overrides and compare changed winner expectations explicitly.

## T5 Warrior audit fixture

`t5-items.json` is the equivalent read-only October 11 snapshot for tokens
30236–30250: 15 tokens, 85 PvE rewards, 85 PvP alternatives, 170 distinct vendor
item/cost paths and 97 referenced equip spells. Each vendor path requires exactly
one corresponding token. The 39 PvE equip spells use family 0; no matching core
spell corrections or installed `spell_dbc` overrides were found. The extended-cost
override table is empty, and there are no quests requiring these 15 tokens.

`test_t5_warrior_token_loot.py` reuses the production converter/scorer fixture with
these real item stats. It checks all five Protection winners, Arms/Fury alternatives,
all three Defender class indices, duplicate priests, non-warrior loot ownership,
all six three-Defender-token class orders per slot, missing presence/plate skill,
and fresh-corpse repeats. World objects, talent-tab lookup, usability and DBC lookups
remain controlled doubles, not a live raid simulation. Current human talent and
proficiency checks are separately documented in
`Documents/t5-warrior-loot-audit-20261011.md`; private character data and raw DBCs
are not included in the public fixture.
