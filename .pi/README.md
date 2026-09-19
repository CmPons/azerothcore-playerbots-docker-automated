# Project-local Pi tooling

Start Pi from the workspace root (`/home/chrisp/Documents/AzerothCore`).
Project settings are cwd-scoped; user-level Pi packages remain separate.

## Subagents removed

`pi-subagents` was uninstalled at the user's request on September 19, 2026,
after the world-bot/chatter deployment completed. `settings.json` no longer
registers it, so Pi will not automatically reinstall it for this project.
Its project-local npm package and unused dependencies were removed.

Use `/reload` in an already-open Pi session to unload the extension still held
in memory. No AzerothCore or Pi bridge restart is needed. Existing session,
review and deployment evidence was retained; unrelated user-level packages
and settings were not changed.

Package caches and historical generated artifacts remain ignored by Git.
