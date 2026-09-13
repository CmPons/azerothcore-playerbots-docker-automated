# Project-local Pi tooling

Start Pi from the workspace root (`/home/chrisp/Documents/AzerothCore`).
Project settings are cwd-scoped; this is not a global installation.

## Subagents

`settings.json` pins `pi-subagents` to **0.67.0**. Pi installs missing project
packages after the project is trusted. To install explicitly from this root:

```bash
npm_config_ignore_scripts=true pi install npm:pi-subagents@0.67.0 -l --approve
```

In an existing Pi session, use `/reload` to load the extension, then
`/subagents-doctor` to inspect setup. No AzerothCore or bridge restart is needed.
Example request: “Ask oracle to review this plan without editing files.”

The package supplies built-in agents; no global agent definitions or model
settings were added. Installation does not launch any AI workers automatically.
Children must follow the applicable `AGENTS.md` instructions, including server
restart restrictions. Keep parallel edits in disjoint files; do not assume Git
worktrees contain this workspace's untracked or modified nested server sources.

Package caches and generated project subagent artifacts are ignored by Git.
The installation was load-checked with Pi 0.84.4 / Node 22.21.1 using an empty
non-interactive session: no load errors and no model requests. Actual child
execution remains to be checked after reloading the interactive session.
