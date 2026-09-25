# Magtheridon channeler strategy deployment — September 25, 2026

## Authorization and scope

User: “Go for build and deploy. I'm signed in but don't worry I will logout after
this message.” No human players were online immediately before stopping.

Deployed [manual channeler control](mag-channeler-manual-control.md): removed six
channeler/summon trigger/action registrations and made the tank-control multiplier
neutral before Magtheridon activates. Boss cubes, positioning, spreading, debris,
timers and boss-phase multipliers remain. Ordinary MT/OT ownership still applies;
this does not order Ari to steal adds already attacking Redshift.

No tuning, NPC mechanics, loot/trade fix, roster maintenance, equipment changes,
SQL migration, forced save or encounter-reset command was included.

## Published build and runtime

- Root build source: `5dc87e6b7ab9361ddc44f3daf4e8141d3c45df6e`.
- Playerbots: `f01eab18ed6dfa4093f9ec4fc5008e59011b315b`.
- Core unchanged: `f6c0fd3cf0d1a3a153bca530cd8efe9efd61b105`.
- Candidate tag: `acore/ac-wotlk-worldserver:mag-channeler-20260925-164903`.
- Image: `sha256:d672a3e5c586835de1a580cf1c13ac569256d62d2503f0f45b75078e88f077a2`.
- Running binary SHA256: `35c9a8048f1830f6bb1f505d9ac658c0911cb8bbd224b4fa92532577aeeb2500`.
- Container: `42a1b9bb13ec1f495761b3749b44c4fb23cdd250bbd1c8638b9aaee0b4956e95`.
- Started: `2026-09-25T14:53:20.811048694Z`.
- Ready: **`2026-09-25T14:53:34.052772269Z`**.
- Rollback tag: `acore/ac-wotlk-worldserver:pre-mag-channeler-20260925-164903`,
  image `sha256:1a18971a2062be9256ccd295818ca875bc36d40c0a040145843a9b5e9c1674a1`.

No rollback performed. Future interruptions require fresh authorization.

## Validation and operations

- Fresh verified four-database/config backup before build and a second verified
  four-database dump after clean shutdown. Backups include stopped group/raid state.
- All maintained repositories published and pins verified. Full build-source
  manifest: 4,206 records, exactly **two** changed production files compared with
  the running support-scaling image: `MagStrategy.cpp`, `MagMultipliers.cpp`.
- Root-owned module mirrors match; all 48 tracked configuration/policy records
  remain identical throughout deployment and to the prior support deployment.
- **13 tests passed**, including 1,728 extracted multiplier cases and a compiled
  historical regression that rejects the old channeler blocker. Two native-header
  syntax checks and the official C++ style checker passed.
- Native Docker image built while the old worldserver continued serving. Candidate
  binary was copied from an inspection container never started as a server.
- Disassembly and ELF string references prove compiled `InitTriggers` registers
  exactly the six retained boss trigger/action pairs. Tank multiplier calls the
  boss-activation helper and no longer calls the channeler lookup helper.
  Retained support-scaling, tank-mode, gem and BG symbols and linked libraries checked.
- Clean exit code 0, then only worldserver recreated using
  `--no-deps --no-build --pull never`; no importer/setup run. The running binary
  matches the candidate exactly. Auth, DB, dependency helpers and Pi were not restarted.
- Post-start observation extended beyond a normal five-minute save interval.
  No OOM, crash or container restart occurred; companion and persistence checks completed.

## Restart side effects — disclosed, not restored

The initial strict audit failed on actual differences; they were investigated,
not silently counted as exact preservation:

1. **Saved raid group 4787 disbanded**, including Redshift's saved main-tank flag.
   The user was told to reform the raid and mark the main tank again. This is the
   same class of startup group cleanup seen in the earlier tank-mode deployment,
   not a new group-management change in this build.
2. **Uncompleted Mag instance 2390 was removed:** zero completed encounters,
   `M L 0`, ten nonpermanent binds and twelve trash respawn records. Cleared trash
   therefore does not persist. Completed Gruul 2353 (`G L 3 3`, mask3), its ten
   permanent binds and its monitored respawn state remain exact. No manual reset
   or restoration was performed.
3. Two mailed **Riding Training Pamphlets** (46875), item GUIDs867011/868704 for
   Meliah/Ari, expired through ordinary startup mail cleanup. They were not in
   inventory/equipment. The stopped dump links them to NPC mails1782/1790, which
   expired at 11:22:22/11:25:41 UTC, before the deployment. Source
   `World::SetInitialWorldSettings`/`MailMgr::ReturnOrDeleteOldMails(false)` deletes
   expired NPC mail attachments. Startup logged 412 expired mails deleted globally;
   these two were the only missing managed item rows. The reviewed audit permits
   exactly these two known mailed items, not arbitrary inventory loss.

No historical profile recovery, item grant or database repair was attempted.

## Preservation review

Startup and post-normal-save snapshots preserve **43 managed characters**,
**2,008 remaining item identities**, and every inventory placement. Levels/XP,
roster, quests, talents, spells, skills and saved AI profiles are exact. No new
profile loss, permanent enchant/gem change or socket fill occurred. Ten temporary
enchant timers advanced normally. The two expired mailed pamphlets and explicit
group/uncompleted-instance losses above are not counted as exact preservation.

All nine core companions remained online at level70, Redshift offline. Raney was
in Arathi Basin under the existing solo-BG policy at the final observation; no BG
or companion relocation command was issued. Core/PB updater ledgers remained
unchanged; auth/database/helper identities and start times and Pi invocation were
unchanged. Pi `/api/tags` was healthy.

## Evidence and limitations

Private evidence: `backups/mag-channeler-deploy-20260925-164903/`, including verified
DB dumps, source/config manifests, binary checks, `audit.py`, build/deploy logs and
explicit group/save/mail exception evidence. None of the private configs, dumps
or binaries is committed.

The nonfatal process-priority permission warning remains. Two low-velocity spline
warnings occurred for creature2974/spawn14054; no movement repair was attempted.
No encounter was forced
to test the change. Source/tests/build establish removal of the override, not that
the next live channeler pull will necessarily be easier. Memory-only `.raidscale`
overrides are lost on restart; configured ten-player defaults are unchanged.
