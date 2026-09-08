# Pi bridge: Luna and an isolated flake runtime

## Active configuration (2026-09-08)

The user approved switching models and restarting only the bridge, explicitly
requiring no changes to `/etc/nixos` and preferring a flake-based runtime.

- Provider/model: `openai-codex` / `gpt-5.6-luna`, using the existing ChatGPT login.
- Pi: **0.84.4**, built from `nix/pi-bridge/flake.nix` and its own `flake.lock`.
- Pinned nixpkgs: `c043004d1c6985732bcc1cbc5a9c9aecbbb4e0f0`.
- Runtime link: `~/.local/share/pi-ollama-bridge/runtime` (Nix build out-link / GC root).
- Service setting: `PI_BRIDGE_PI_BIN=%h/.local/share/pi-ollama-bridge/runtime/bin/pi`.
- Restart: **2026-09-08 20:45:48 CEST**, PID **675878** at activation.
- Limits unchanged: **12/minute, one concurrent request, no hourly cap**, thinking
  off, timeout 75 seconds. Busy requests may still return empty responses.

`/etc/nixos` was not edited and no `nixos-rebuild` or system/Home Manager switch was
run. Root `flake.nix` and the user's unrelated root `flake.lock` were not edited;
their SHA256 hashes match before/after. AzerothCore worldserver/auth/database
images, start times and restart counts also match before/after exactly.

## Why the change

The old bridge ran Pi **0.79.1** from `/etc/profiles/per-user/chrisp/bin/pi` and
requested `gpt-5.4-mini`. On September 8, upstream requests repeatedly failed with:

```text
The 'gpt-5.4-mini' model is not supported when using Codex with a ChatGPT account.
```

The HTTP health endpoint still worked, so that alone did not demonstrate usable
chatter generation. Pi 0.79.1's catalog did not list Luna; the separately available
0.84.4 runtime did. We did not update the user's global Pi installation.

OpenAI's [ChatGPT/Codex help](https://help.openai.com/en/articles/11369540-using-codex-with-chatgpt)
points mini users to Luna; [Luna's model page](https://developers.openai.com/api/docs/models/gpt-5.6-luna)
positions it for cost-sensitive work. API token prices are not a measurement of
this account's included Codex allowance. No API key or separate billing provider
was introduced.

## Reproduce the runtime

From the repository root:

```bash
mkdir -p "$HOME/.local/share/pi-ollama-bridge"
nix build "path:$PWD/nix/pi-bridge#pi" \
  --out-link "$HOME/.local/share/pi-ollama-bridge/runtime"
"$HOME/.local/share/pi-ollama-bridge/runtime/bin/pi" --version
```

The explicit `path:` reference copies only this small runtime-flake directory,
not the large server tree, private `.env` files or backups. Nix uses the dedicated
lock and immutable revision; no per-request Nix evaluation/download is needed.
Systemd runs the already-built executable directly. Keep the out-link so garbage
collection does not remove the runtime.

Updating the bridge's Pi version later means reviewing a new revision in this
small flake, refreshing its lock, and rebuilding the out-link. Do not update the
root flake or `/etc/nixos` for that purpose. Service interruptions still require
fresh approval. Changing the out-link affects the next Pi child invocation even
without restarting the Python bridge, so do runtime updates deliberately too.

The tracked `scripts/pi-ollama-bridge.service.example` matches the installed unit.
After building the runtime, installing that example and reloading/restarting the
user unit requires explicit restart permission, not a worldserver restart.

## Verification and evidence

- Two isolated Luna prompts through Pi 0.84.4 succeeded with existing Codex auth,
  taking **6.38s** and **7.14s**. They used the bridge's command construction and
  sanitizer, without tools, sessions, extensions or context files.
- Example tank reply: `On the loose one. Try not to lose the skull.`
- The pinned flake evaluated and built Pi 0.84.4 successfully; `--version` verified
  the actual user-local runtime.
- `systemd-analyze --user verify` passed for the edited service.
- After the approved restart, `/api/tags` advertised `gpt-5.6-luna` and a real
  `/api/generate` request returned **`bridge luna ok`** on the first attempt in
  **4.77 seconds**. This response went only to the test client, not in-game chat.
- Startup confirmed the expanded user-local executable, provider/model and limits.
  Early post-restart logs show occasional expected busy rejections, not the old
  unsupported-model errors. Actual in-game delivery remains a separate check.
- Thirteen automated tests pass (mocked generation/admission plus service/flake
  pin assertions); they do not spend quota or restart any service:

```bash
python3 -m unittest discover -s scripts/tests -p test_pi_ollama_bridge.py -v
```

Private backup/evidence: `backups/pi-bridge-luna-20260908-204405/`, including old
service copies, root-flake hashes, before/after process/container metadata,
activation logs, active runtime settings and the successful HTTP response.

Reverting to the old unit would restore the known-rejected mini model, so that is
not a working fallback. Future rollback/model changes should first be tested
with the same account and then activated with fresh bridge-restart approval.
