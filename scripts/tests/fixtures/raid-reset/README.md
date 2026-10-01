# Expansion reset layout contracts

`layouts.json` lists the intended required, optional and ignored **symbolic encounter
names** for the 17 newly supported BC/Wrath maps. These are not DungeonEncounter.dbc
kill-credit indices. Onyxia is covered by the existing Classic-map test.

`test_expansion_raid_reset.py` extracts and compiles the real enums/count constants from
the maintained native headers, comparing their masks and headers with the production
policy. It also checks the actual instance save writers: base boss states first,
legacy VoA states, or Trial's checkpoint. New/renumbered slots fail these contracts.

The source audit started at core `f6c0fd3cf0d1a3a153bca530cd8efe9efd61b105`; tests always
read the current maintained source. No credentials, character records, database dumps
or client DBC files are included. Test save strings are synthetic.

The Chess runtime test injects the production SetData Chess branch and combat predicate
into a small C++ fixture. World/DB access is doubled; the actual first-victory gate and
uninitialized-slot save fallback execute unchanged. It is not a full server simulation.
