# lib/

## Purpose

- Runtime modules imported by `main.py` on the ESP32 target.
- Target runtime is MicroPython; modules that touch hardware are imported lazily or only on device.
- Pure modules must import under CPython. Hardware imports belong in transport adapters and stay lazy.

## Ownership

- Owns mesh transport, wire protocol, configuration parsing/validation, gateway storage, egress, HTTP/WebSocket API, SX1262 uplink/receiver, and HID/key logging helpers.
- Does not own boot decisions (root `main.py`), example configs (`configs/`), or user docs (root).

## Local Contracts

- Transport: `mesh_network.py` (`MeshNetwork`) owns peer allowlisting, routing, ACKs, bounded outbox, persistence, and failover counters. `mesh_transport.py` defines the radio interface; `espnow_transport.py` owns ESP-NOW hardware setup and pre-registers every configured encrypted peer before receiving. Tests inject a fake transport.
- Protocol: `mesh_protocol.py` — pure helpers, `PROTOCOL_VERSION = 3`, packet types `r`/`d`/`a`, 240-byte cap, event allowlist, decode validation and duplicate cache/route selectors.
- Config: `settings.py` (`Settings`) — reads `settings.json`, exposes flat attributes, validates roles, `mesh_only` constraints, secrets, peer key/MACs, egress values. `is_placeholder_secret()` is shared with `api.py`.
- Gateway storage: `mesh_store.py` appends NDJSON. `egress.py` batches to `log`, `wifi`, or native SX1262 `lora`; the queue is bounded, persisted, and retried.
- LoRa: `lora_frame.py` owns HMAC-authenticated fragmentation/ACK frames; `lora_link.py` owns half-duplex stop-and-wait reassembly/retry; `sx1262.py` is the hardware adapter for the Seeed 113110064 B2B pinout, including RF switch and 1.8 V TCXO control; `lora_sink.py` owns receiver dedup, enrichment, retention, inventory, and bounded per-implant text reconstruction; `lora_web.py` owns its authenticated API.
- API: `api.py` (`DuckLoggerAPI`) — routes `/`, `/log`, `/script`, `/kbd` (WebSocket), `/settings`, `/mesh/status`, `/mesh/log`, `/gateway/config`; token auth via `_require_auth`; gateway configuration accepts only allowlisted management/egress/LoRa fields, validates the pending settings file, preserves secrets, and requires reboot.
- HID/key path (device only): `keyboard.py`, `keyframe.py`, `mapper.py`, `duckyscript.py`, `logger.py`, `key_led.py`, `uart_buffer.py`. `logger.Log` produces the decoded press string used by `key_event`.
- Radio/network: `wifi_radio.py` dispatches to `access_point.py` (AP mode) or `wifi.py` (STA mode).
- Local helpers: `queue.py` is the MicroPython-safe queue used by the API.
- `microdot/` is vendored third-party code (Microdot framework); do not reformat or "improve" it. Change it only when a specific fix is required, and isolate the change.
- Import style: `try: import ujson` with `json` fallback; no new third-party dependencies; avoid blocking I/O in coroutines.

## Work Guidance

- Keep protocol changes backward-aware: bump `PROTOCOL_VERSION` when the wire format or validation changes, and update `tests/test_mesh_protocol.py`.
- Never broaden the event allowlist or let `key_event` carry arbitrary content.
- Keep `Settings` validation defensive: reject placeholders, unsafe `mesh_only` combinations, and inconsistent egress settings.
- Preserve public attribute names on `Settings`; `main.py` and tests read flat attributes.
- Add new radios by implementing `RadioTransport`; do not branch on radio type inside mesh routing or receive logic.

## Verification

- `python -m pytest -q` from the repo root (covers protocol, settings, store, egress, API, logger, mesh network).
- `python -m compileall -q lib`.
- Hardware-only modules are not covered by tests; verify them on a board.

## Child DOX Index

- `microdot/AGENTS.md` — vendored Microdot HTTP/WebSocket framework; treat as read-only unless fixing a targeted bug.
