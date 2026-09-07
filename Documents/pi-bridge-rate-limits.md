# Pi chatter bridge admission fix (2026-09-07)

## Status

**Active after the explicitly approved bridge-only restart.**

- Restarted: 2026-09-07 18:34:10 CEST; PID 162731.
- Startup confirmed `max_concurrent=1; rate=12/min unlimited/hour`.
- GET `/api/version` succeeded; no deliberate model generation was used for the check.
- Worldserver image, start time, and restart count matched before/after exactly.
- Real in-game chatter delivery after activation remains to be observed.

## Diagnosis

The bridge was healthy but returning HTTP 200 with empty replies when its local
rolling-hour limit was exhausted. At the investigation snapshot, the preceding
30 minutes contained 102 hourly rejections, and the preceding 60 minutes contained
31 concurrency rejections. No Pi process-exit/timeout errors were seen in that
window. Successful generations are not logged with debug disabled, so these are
rejection counts, not a complete measure of model usage.

A confirmed accounting bug charged minute/hour counters before acquiring the
single generation slot. A busy request never invoked Pi but still consumed quota.
Worldserver allows three concurrent chatter requests, whereas the bridge's
configured concurrency is one, making this failure path relevant.

## Changes

- `scripts/pi_ollama_bridge.py`: the hourly cap defaults to **0 (disabled)**.
  A positive `PI_BRIDGE_MAX_PER_HOUR` remains supported as an opt-in safeguard.
- Both `scripts/pi-ollama-bridge.service.example` and the installed
  `~/.config/systemd/user/pi-ollama-bridge.service` explicitly set:
  - `PI_BRIDGE_MAX_PER_HOUR=0`
  - `PI_BRIDGE_MAX_PER_MINUTE=12` (unchanged)
  - `PI_BRIDGE_MAX_CONCURRENT=1` (unchanged)
- Acquire a generation slot before rate admission. A busy rejection consumes no
  quota. Minute/hour admission is atomic under its existing lock; rejected
  admission adds no timestamps and always releases the worker slot.
- Use monotonic time for rolling windows, expiring entries at exactly 60/3600
  seconds. Wall-clock changes cannot extend a window unexpectedly.
- Startup prints `rate=12/min unlimited/hour` for this service configuration.

Quota measures **admitted invocation attempts**, not delivered chat messages.
Once a Pi invocation is attempted, failures/timeouts still count: an upstream
request may already have consumed resources. Busy and rate-limit rejections do
not count. This change does not queue or retry rejected requests, increase
concurrency, change models, or disable the minute guardrail. Without an hourly
cap, sustained generation usage can be higher than before (bounded by the minute
limit, single worker, and upstream limits).

## Verification

```bash
python3 -m unittest discover -s scripts/tests -p 'test_pi_ollama_bridge.py' -v
```

Twelve tests cover busy rejection, no quota consumed by rate rejections, semaphore
release on all exit paths, exact window boundaries, monotonic time, atomic
multi-worker admission, and more than 120 generations in an hour with the cap
disabled. Pi is mocked; tests never invoke the model or change the live service.

Pre-change service/script copies and process metadata are retained privately in
`backups/pi-bridge-quota-20260907/`. Git preserves the tracked rollback versions.

Activation procedure completed with explicit permission:

1. `systemctl --user daemon-reload`
2. `systemctl --user restart pi-ollama-bridge.service`
3. Verify active status, the new startup limits, and GET `/api/version` (no model
   invocation). Confirm worldserver's image/start time is unchanged.
4. Observe real chatter (pending). Busy/minute rejections may still occur; hourly
   rejections should not occur with the new configuration. Further service
   interruptions require fresh permission.
