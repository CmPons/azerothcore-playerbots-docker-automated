# Twin Emperors: learning from the legitimate kill

## Status

September 12, 2026: the user reported a **legitimate Twin Emperors kill on the existing live build**,
before either 0028 or this follow-up was deployed. Earlier attempts reached 50% and 190k remaining.
The strategy improvements are learning from those attempts, not prerequisites credited for that kill.

The user authorized deployment while taking a break, with Ouro planned for tomorrow. Native build,
backup/save verification and a logged-out gate must pass before the worldserver-only restart.
No encounter reset, database repair, gear change or additional difficulty reduction is authorized.

Canonical incremental patch: `patches/0029-playerbot-twins-caster-victim-coverage.patch`, after 0028.
Five production files in the core's playerbots tree: `Aq40Coordination.cpp`, `Aq40Helpers.{cpp,h}`,
`Aq40Actions_Coordination.cpp`, and `Ai/Base/Value/PartyMemberToHeal.cpp`.
The patch is derived from preserved actual working sources, including patch-managed untracked files.

## Healer assignments

| Healer | Assignment |
|---|---|
| Meliah | Ari's physical station through both directions of the teleport |
| Keilmere | Redshift's physical station through both directions of the teleport |
| Ailina | Whoever Vek'lor is actually attacking; Beliona if there is no eligible current victim |

This supersedes 0028's intermediate Ailina/Keilmere allocation. Priests are preferred for the first
two station slots, then GUID order remains deterministic; names are not hardcoded. It is a local
assignment preference, not a claim that priests universally outheal druids. Dead healers retain
ordering: Ailina can fill a vacancy without moving the surviving dedicated priest to the other tank.

Local healing triage gives an injured actual caster victim the same preference as the station tank,
without double-counting when those are the same player. Critical nearby emergencies and explicit
focus-heal commands retain precedence. No instant/free heals, spell changes or extra mana are added.

## Actual-victim movement

Pilbok, Kaaren, Raney, or any other eligible bot holding caster aggro can use the existing Twins
positioning action. This no longer requires the bot to be the designated caster tank. Beliona's
threat recovery is not assumed or forced.

- Stay put when safely within a living, same-phase/group healer's buffered range and LOS.
- Otherwise seek a safe local healing overlap around Vek'lor, using the healer's actual AI range
  and configured healing distance, capped at 34 yards with a buffer. Passive/charmed/dead healers
  do not authorize a destination; a healer victim may cover itself.
- Keep ordinary victims within a 40-yard center-distance envelope, inside the native 45-yard chase
  threshold. The designated caster prefers 28 yards so ordinary ranged tank spells remain usable.
- Use at most 6-yard waypoints. Probe native navigation, require a normal complete path and reject
  long detours, outward range violations, LOS failures and paths through Arcane Burst/bombs/Blizzard.
  Check entire path segments, allowing monotonic escape when already inside a hazard.
- Preserve the checked waypoint coordinates through native `MoveTo`; do not invoke generic endpoint
  relocation. The movement remains native pathfinding, not teleporting or forced destinations.
- Arcane, bomb and Blizzard escapes for the actual caster victim use this same bounded routing.
  Generic DPS reach/flee/contact cannot immediately undo it. Old automatic movement can settle;
  active higher-priority manual movement leases and CC/passive controls retain precedence.
- If there is no safe healing overlap while already safely holding the caster, do not run across
  the room after a remote healer. Hold locally while flexible healer coverage approaches.
- If aggro was already acquired outside the envelope, move inward without increasing caster range.
  This cannot undo a chase that the native boss has already begun.
- Remove the old deliberate long-range caster relocation for boss separation. The physical victim
  separates Vek'nilash; a bot holding both bosses must not drag them together.
- Stop applying actual-victim routing when aggro changes. Do not automate the human's movement.
  Outside an active Twins encounter, normal encounter/healing behavior remains unchanged.

Boss health, damage, teleport timing, shared health, mutation mechanics, threat and Heal Brother are
untouched. These checks do not guarantee healing throughput or zero boss motion under live latency,
knockbacks, moving healers, or a boss position change between AI updates.

## Verification

The focused suite compiles actual production helpers/actions/triage against offline API doubles,
with UBSan and warnings as errors. It covers both station swaps and healer deaths; temporary
Pilbok/Kaaren/Raney aggro; injured victim triage versus station emergencies; coverage acquisition,
settling and aggro release; remote/passive/charmed healers; range/LOS/path rejection; short hazard
escapes; inward recovery from pre-existing long range; both-boss ownership; human/passive/CC guards;
and active versus expired manual leases.

Patch round-trip and focused pinned replay include 0028 then 0029. The known unrelated 0021 replay
failure remains explicitly excluded from the focused replay; no live source reset-to-pins is used.
The selected regression suite passes: 90 tests, 89 passed and one optional MySQL fixture skipped.
Scoped production C++ style and added-line width/whitespace checks pass. Doubles and native
compilation are not a live pathfinding, cast-latency, HPS or subsequent-pull simulation.

Preparation backup: `backups/twins-caster-coverage-preparation-20260912-202803/`.
