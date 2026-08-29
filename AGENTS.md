# Agent Instructions

## Hard rule

Do NOT restart, recreate, stop, or otherwise interrupt the AzerothCore server unless the user explicitly says to do so.

Keep git clean after each change so we can easily revert: inspect diffs/status, commit completed work, and push it to the fork/remote when network credentials are available.

Building images, editing files, reading logs, and checking status are OK. If a change requires a restart to take effect, tell the user that and wait for permission.

## AzerothCore server operations

Project root: `/home/chrisp/Documents/AzerothCore`

Main server tree: `azerothcore-wotlk/`

Common commands:

```bash
cd /home/chrisp/Documents/AzerothCore/azerothcore-wotlk

# Show containers
docker compose ps

# Follow worldserver logs
docker compose logs -f ac-worldserver

# Show recent worldserver logs
docker compose logs --tail=200 ac-worldserver

# Build worldserver image without restarting it
docker compose build ac-worldserver

# Start server only if user explicitly asks
docker compose up -d ac-worldserver

# Restart server only if user explicitly asks
docker compose restart ac-worldserver

# Recreate server container only if user explicitly asks
docker compose up -d --force-recreate ac-worldserver

# Stop server only if user explicitly asks
docker compose stop ac-worldserver
```

Readiness log line to look for:

```text
worldserver-daemon) ready...
```

Useful log checks:

```bash
docker compose logs --tail=300 ac-worldserver | rg "PlayerbotChatter|WORLD: World Initialized|ready|ERROR|FATAL"
```

## Source tree sync note

There are two module source locations that should stay synced when editing playerbot chatter:

- Root working copy: `modules/mod-playerbot-chatter/src/`
- AzerothCore build tree: `azerothcore-wotlk/modules/mod-playerbot-chatter/src/`

After editing root module files, sync them into the build tree before building, for example:

```bash
rsync -a modules/mod-playerbot-chatter/src/PBChatterObserver.cpp \
  modules/mod-playerbot-chatter/src/PBChatterAmbientPrompt.cpp \
  azerothcore-wotlk/modules/mod-playerbot-chatter/src/
```

## Playerbot chatter / Pi bridge overview

AzerothCore `mod-playerbot-chatter` talks to an Ollama-compatible HTTP endpoint. Locally, that endpoint is provided by the Pi bridge instead of a real Ollama model.

Bridge script:

```text
scripts/pi_ollama_bridge.py
```

Systemd user service example:

```text
scripts/pi-ollama-bridge.service.example
```

Installed user service:

```text
~/.config/systemd/user/pi-ollama-bridge.service
```

Current expected bridge behavior:

- Listens on `0.0.0.0:11435`
- Exposes Ollama-shaped `/api/generate`
- Calls `pi --print` behind the scenes
- Uses provider/model via env, currently expected as:
  - `PI_BRIDGE_PROVIDER=openai-codex`
  - `PI_BRIDGE_MODEL=gpt-5.4-mini`
- AzerothCore chatter points at:
  - `http://192.168.1.7:11435/api/generate`

Bridge commands:

```bash
# Check bridge status
systemctl --user status pi-ollama-bridge.service

# Follow bridge logs
journalctl --user -u pi-ollama-bridge.service -f

# Test bridge
curl -s http://127.0.0.1:11435/api/generate \
  -H 'Content-Type: application/json' \
  -d '{"model":"test","prompt":"say bridge ok","stream":false}'
```

Do not restart the bridge unless needed for the user's request. If bridge config changes require a restart, ask first.

## Config and generated files cheat sheet

Primary env/config source:

```text
azerothcore-wotlk/.env
```

Runtime config files commonly edited/checked:

```text
azerothcore-wotlk/env/dist/etc/worldserver.conf
azerothcore-wotlk/env/dist/etc/modules/playerbots.conf
azerothcore-wotlk/env/dist/etc/modules/mod_playerbot_chatter.conf
```

Module dist config templates:

```text
modules/mod-playerbot-chatter/conf/mod_playerbot_chatter.conf.dist
azerothcore-wotlk/modules/mod-playerbot-chatter/conf/mod_playerbot_chatter.conf.dist
```

Important local source edits from this workspace:

```text
modules/mod-playerbot-chatter/src/PBChatterObserver.cpp
modules/mod-playerbot-chatter/src/PBChatterAmbientPrompt.cpp
modules/mod-playerbot-chatter/src/PBChatterEvents.cpp
modules/mod-playerbot-chatter/src/PBChatterContext.cpp
azerothcore-wotlk/src/server/game/Entities/Player/KillRewarder.cpp
```

If changing `.env` knobs that setup writes into configs, normally run setup/apply step only when appropriate, then tell the user a worldserver restart is needed. Do not restart without explicit permission.

## Chatter behavior notes

- Reactive chatter is triggered by real player say/yell, whisper, party, or raid messages.
- Ambient chatter only has useful context when real players are present/active; do not worry about Codex/Pi drain from an empty server unless logs show requests.
- Playerbot command-like messages are filtered by `PBChatterClassifier::IsCommand` using `PlayerbotChatter.CommandKeywords`.
- `summon` is included in command keywords and should not trigger reactive chatter.
- Ambient buffering also filters command-like messages, so commands should not later become ambient context.
- `LANG_ADDON` traffic is ignored to avoid DBM/Recount/MultiBot addon spam becoming AI chat.

Useful config key:

```text
PlayerbotChatter.CommandKeywords
```

Runtime current expected chatter URL:

```text
PlayerbotChatter.OllamaUrl = http://192.168.1.7:11435/api/generate
```

## Playerbot command reminders

Commands can be whispered to a bot or sent in party depending on playerbot handling. Examples:

```text
/w Arinerica help
/w Arinerica who
/w Arinerica stats
/w Arinerica quests
/w Arinerica quests all
/w Arinerica quests incompleted
/w Arinerica quests completed
/w Arinerica summon
```

Useful meanings:

- `who`: class/race/level/spec-ish info, gear score, location/master
- `stats`: money, bag slots, durability, XP percent/rested percent
- `quests`: quest log summary/listing
- `summon`: playerbot command; chatter should ignore it

Current playerbot command prefix is empty and separator is `\\`:

```text
AiPlayerbot.CommandPrefix = ""
AiPlayerbot.CommandSeparator = "\\\\"
```

## Gameplay/local tweak notes

Open-world PvP/playerbot kills were locally changed to grant XP in:

```text
azerothcore-wotlk/src/server/game/Entities/Player/KillRewarder.cpp
```

The local gate currently includes player victims in XP reward eligibility while leaving creature-specific reputation/kill-credit behavior PvE-only.

Key `.env` gameplay knobs already present include:

```text
XP_KILL_RATE=3
XP_QUEST_RATE=3
XP_EXPLORE_RATE=3
REPUTATION_RATE=5
HONOR_RATE=5
MAX_PLAYER_LEVEL=60
RANDOM_BOT_MAX_LEVEL=60
BOT_LEVEL_BRACKET_EXCLUDE_NAMES=Meliah,Arinerica
```

## Database access reminders

Database container service is usually `ac-database`. Root password is in `.env` as `DOCKER_DB_ROOT_PASSWORD`.

Useful read-only style checks:

```bash
cd /home/chrisp/Documents/AzerothCore/azerothcore-wotlk
source .env

docker compose exec ac-database mysql -uroot -p"$DOCKER_DB_ROOT_PASSWORD" -e "SHOW DATABASES;"

docker compose exec ac-database mysql -uroot -p"$DOCKER_DB_ROOT_PASSWORD" acore_characters \
  -e "SELECT guid,name,level FROM characters WHERE name IN ('Meliah','Arinerica');"
```

Do not run destructive SQL unless the user clearly asks.

## Build/restart policy quick reference

Safe without asking:

```bash
docker compose build ac-worldserver
docker compose ps
docker compose logs --tail=200 ac-worldserver
systemctl --user status pi-ollama-bridge.service
```

Ask first / only if user explicitly requests:

```bash
docker compose restart ac-worldserver
docker compose up -d ac-worldserver
docker compose up -d --force-recreate ac-worldserver
docker compose stop ac-worldserver
systemctl --user restart pi-ollama-bridge.service
```
