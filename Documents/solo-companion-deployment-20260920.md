# Solo companion BG / friend cap / chatter deployment — September 20, 2026

The user authorized deployment of the three staged changes, then explicitly chose
**cap15** after preflight found eleven eligible bot friends rather than nine.
Only the worldserver was stopped/recreated. Ready: **2026-09-20 07:47:48.753746006 UTC**
(09:47:48 local). Further interruptions require fresh permission.

## Active changes

- [Solo companion BG admission](solo-companion-battlegrounds.md): ordinary BGs while
  free/solo; no party/raid extraction, including queue-to-port races; no arena drafting.
- [Friend-protection cap15](roster-world-bots.md#friend-protection-cap--15-active), up
  from5 live. Both private env copies changed from the staged9 to15. The account has
  eleven eligible bot friends including Alenaron/Emia;9 would exclude Feelesia by GUID
  order. Fifteen covers all eleven and leaves four slots of headroom.
- [Bare `stats` chatter suppression](chatter-command-filter.md): appended to the live
  command list without removing/reordering any existing keywords.

The automatic solo-healer correction is **not implemented**. Ailina's and Keilmere's
manually saved `healer dps` strategies survived this restart unchanged.

## Revisions, build and recovery

- Core: `b6e03792268af131467f46f2e7455dc5e1e82c5d` (unchanged).
- Playerbots: `a72a2ac66c2d438daaef5e48fd50751eb63cd81f`.
- Root build-source acceptance: `68f1c2a8387a6d370008d7c57841f598758ce625`.
- Cap15 example/test update: root `29ef8d7` (published before deployment).
- Image tag: `acore/ac-wotlk-worldserver:solo-bg-20260920-092943`, also `:master`.
- Image ID: `sha256:1c43fb094c0f397f145c1055d63986f5f4993931ae7ab6ba7c95373c38465601`.
- Running binary SHA256: `a7b1e352b827966223e914f9715fa98ea50fc5588f13ab2eeeaef6e9e3a73c9c`.
- Rollback tag: `acore/ac-wotlk-worldserver:pre-solo-bg-20260920-092943`, preserving
  image `sha256:ceef54dc2c9c511651e5fd63d0597490fd452e2f501f8b250e2645277b33ca56`.
- Private evidence/recovery directory: `backups/solo-bg-deploy-20260920-092943/`.

Native build/link ran 09:31:18–09:43:55 local against the same pinned Ubuntu base and
build arguments as the prior deployment. The running server stayed untouched during
building. Nineteen focused tests passed, including cap15 through the real setup setter.
The final binary contains the BG policy and all three queue/arena/port hooks. Bounded
disassembly confirms repository loading precedes solo-strategy restoration. An isolated,
networkless `ldd` check found no missing dependencies; no server entrypoint was run.

The initial disassembly check used a mangled selector together with demangling and
returned no function body. This inspection-tool error was corrected with the demangled
selector and rechecked successfully; the image itself built successfully and was not
changed/rebuilt. Both the initial failure and subsequent acceptance are retained.

All 5,031 source/Git manifest records stayed unchanged across build/deployment. The
non-Git delta from the previous deployed source was exactly six playerbot files and
the two chatter default/template files. Custom module mirrors matched. Of 48 config,
env and policy records, only four authorized files changed: the cap in both env copies,
the live playerbots cap, and the live chatter keyword list. All other bytes/modes matched.

Deployment used:

```sh
docker compose up -d --no-deps --no-build --pull never --force-recreate ac-worldserver
```

No full setup/update, helper startup, DB import, gameplay SQL write, bot command,
level sync, profile reset or gear reroll was performed. Rollback would require fresh
permission and consideration of the backed-up configs, not merely retagging an image.
Never restore an old database over newer gameplay.

## Preservation and explicit exceptions

- Verified fresh four-database dumps before build and after clean shutdown. The stopped
  dump plus snapshots/checksums are the recovery authority. No human account was online
  immediately before shutdown; old worldserver exited0, without OOM.
- Stopped/start levels/XP, quests, saved strategy profiles, roster, and instance/bind/AQ
  respawn snapshots matched exactly. No historical instance state was restored.
- **Inventory:** all pre-existing inventory/item rows and equipped items were identical.
  Normal login guild initialization added Guild Tabard5976 to a bag slot for Keilmere
  and Feelesia. Exactly two inventory rows and two item rows were added; none removed
  or replaced. The same existing login behavior was observed at the prior deployment.
- **Groups:** as flagged before stopping, existing `KeepAltsInGroup=0` caused Ari and
  Meliah to leave offline Redshift's party324 during login; the party disbanded. This
  setting was not changed. No group commands or SQL edits were issued. This is login
  cleanup, not the BG queue-cancellation policy disbanding a party.
- All nine bots were confirmed online after initial login scheduling settled. Redshift
  was offline at that check, then returned and formed party31 with Ari/Meliah during
  later read-only observation. No further interruption was performed. Being online
  is not, by itself, proof of sustained productive activity.
- Auth/database/importer/client-data container identities, start times and states stayed
  unchanged. Pi bridge PID/invocation/start time matched; `/api/tags` was healthy. No
  bridge restart or model generation test was performed.
- Core updater remained disabled. The separate playerbots updater stayed enabled with
  all28 applied files matching included SQL source. Both updater ledgers were unchanged.
- Level70 caps, selected GUIDs, bracket exclusions and installed raid policy were retained.
  New worldserver restart count0. Existing process-priority permission and missing
  chatter GroupJoin-config diagnostics also appeared in the previous deployment.

## Validation limits

Production-body fixtures cover the companion policy, party/raid leaders and members,
queue cancellation, packet-boundary race protection, arenas, manual orders and ordinary
human/filler behavior. The native image built/linked and the running binary matched.
No real queued-bot recruitment race or in-game `stats` request was injected during
this deployment. Those live behavior checks remain distinct from source tests and
successful startup. No XP-rate or eventual level70 guarantee is implied.
