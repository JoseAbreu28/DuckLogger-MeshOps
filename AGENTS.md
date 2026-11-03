# DOX framework

- DOX is a highly performant AGENTS.md hierarchy installed here
- The agent must follow DOX instructions across any edits

## Core Contract

- AGENTS.md files are binding work contracts for their subtrees
- Work products, source materials, instructions, records, assets, and durable docs must stay understandable from the nearest applicable AGENTS.md plus every parent AGENTS.md above it

## Read Before Editing

1. Read the root AGENTS.md
2. Identify every file or folder you expect to touch
3. Walk from the repository root to each target path
4. Read every AGENTS.md found along each route
5. If a parent AGENTS.md lists a child AGENTS.md whose scope contains the path, read that child and continue from there
6. Use the nearest AGENTS.md as the local contract and parent docs for repo-wide rules
7. If docs conflict, the closer doc controls local work details, but no child doc may weaken DOX

Do not rely on memory. Re-read the applicable DOX chain in the current session before editing.

## Update After Editing

Every meaningful change requires a DOX pass before the task is done.

Update the closest owning AGENTS.md when a change affects:

- purpose, scope, ownership, or responsibilities
- durable structure, contracts, workflows, or operating rules
- required inputs, outputs, permissions, constraints, side effects, or artifacts
- user preferences about behavior, communication, process, organization, or quality
- AGENTS.md creation, deletion, move, rename, or index contents

Update parent docs when parent-level structure, ownership, workflow, or child index changes. Update child docs when parent changes alter local rules. Remove stale or contradictory text immediately. Small edits that do not change behavior or contracts may leave docs unchanged, but the DOX pass still must happen.

## Hierarchy

- Root AGENTS.md is the DOX rail: project-wide instructions, global preferences, durable workflow rules, and the top-level Child DOX Index
- Child AGENTS.md files own domain-specific instructions and their own Child DOX Index
- Each parent explains what its direct children cover and what stays owned by the parent
- The closer a doc is to the work, the more specific and practical it must be

## Child Doc Shape

- Create a child AGENTS.md when a folder becomes a durable boundary with its own purpose, rules, responsibilities, workflow, materials, or quality standards
- Work Guidance must reflect the current standards of the project or user instructions; if there are no specific standards or instructions yet, leave it empty
- Verification must reflect an existing check; if no verification framework exists yet, leave it empty and update it when one exists

Default section order:

- Purpose
- Ownership
- Local Contracts
- Work Guidance
- Verification
- Child DOX Index

## Style

- Keep docs concise, current, and operational
- Document stable contracts, not diary entries
- Put broad rules in parent docs and concrete details in child docs
- Prefer direct bullets with explicit names
- Do not duplicate rules across many files unless each scope needs a local version
- Delete stale notes instead of explaining history
- Trim obvious statements, repeated rules, misplaced detail, and warnings for risks that no longer exist

## Closeout

1. Re-check changed paths against the DOX chain
2. Update nearest owning docs and any affected parents or children
3. Refresh every affected Child DOX Index
4. Remove stale or contradictory text
5. Run existing verification when relevant
6. Report any docs intentionally left unchanged and why

## User Preferences

- Respond to the user in Spanish when they write in Spanish; keep code, identifiers, and docs in English.
- Do not commit, push, or open PRs unless the user explicitly asks.

# DuckLogger MeshOps

## Purpose

- Research prototype that adapts DuckLogger into a local multi-hop ESP-NOW telemetry mesh: nodes originate and relay telemetry, one gateway is the root and stores events.
- The gateway can forward authorized telemetry over SX1262 to an external `lora_sink` that provides local analysis and bounded logs.
- Scope is authorized engagements and isolated lab testing only. Documentation and examples must keep that authorization warning.
- Authorized event kinds are exactly `heartbeat`, `synthetic_counter`, `link_test`, and `key_event` (decoded press string, max 120 chars).

## Ownership

- Root owns device roles, boot flow, configuration schema, mesh contracts, and release/CI rules.
- `lib/` owns runtime modules (MicroPython target, CPython-testable where possible).
- `tests/` owns the CPython pytest suite.
- `configs/` owns canonical example configurations.
- Web UI (`index.html`), provisioning (`flasher.py`, `installer.sh`), backup archives, and docs (`README.md`, `MESHOPS.md`, `ROADMAP.md`, `resources.md`) are root-owned artifacts.

## Local Contracts

- `main.py` is device-only: it imports `machine`, `neopixel`, and `usb-device` modules and cannot run under CPython. Testable logic lives in `lib/`.
- Two boot paths: `device.mesh_only: true` initializes no USB HID; the default path keeps upstream DuckLogger HID behaviour and may attach mesh publishing.
- `settings.json` is the single device config, validated by `lib/settings.py`. It rejects placeholder secrets (`CHANGE_ME_*`, short AP password/token, missing/short `mesh.peer_key`, empty `mesh.peer_macs`).
- Root `settings.json` uses the legacy flat standalone schema (`mode`/`ssid`/`password`/`api_token`); `configs/*.json` use the nested MeshOps schema. `Settings` falls back to the flat keys for management.
- Mesh roles (`node`/`gateway`) require explicit `mesh_only: true` unless `allow_hid_with_mesh: true`; `data_source` is node-only and needs `mesh_only` + `mesh.enabled`.
- `mesh.peer_macs` is a required allowlist; packets from unknown MACs are dropped before decode/relay. All devices share `mesh.network_id`, `mesh.channel`, and the 16-byte `mesh.peer_key`.
- Protocol: `lib/mesh_protocol.py` (`PROTOCOL_VERSION = 3`, types `r`/`d`/`a`, 240-byte cap, event allowlist). `lib/mesh_network.py` uses the interface in `lib/mesh_transport.py`; `lib/espnow_transport.py` is the only mesh radio adapter. LoRa is only a gateway egress backend, not a mesh radio.
- Gateway persists NDJSON (`mesh.log_path`) and exposes `/mesh/status`, `/mesh/log`, and authenticated `/gateway/config`; auth accepts `Authorization: Bearer`, `?token=`, or `X-API-Key` with `management.api_token`.
- Egress backends: `none`, `log`, `wifi`, `lora`; LoRa uses authenticated fragmented frames over a native SX1262 with ACK/retry. The canonical LoRa hardware is the Seeed 113110064 XIAO ESP32S3 + Wio-SX1262 B2B kit. `wifi` requires `management.enabled`; the bounded queue is persisted and retried with exponential backoff.
- Deployed gateways default to LoRa egress. The telemetry console may observe gateway status but must not switch egress; Wi-Fi egress is an explicit manual operator configuration.
- `lora_sink` is an external SX1262 receiver role. It exposes an authenticated AP web console, identifies implants, derives direct/relay paths, and keeps bounded rotating NDJSON logs.
- Runtime code must stay MicroPython-compatible: no third-party packages, guard optional imports, keep memory allocations small.
- Generated/runtime artifacts are not documented as source: `gateway_m5sticks3/.pio/`, `gateway_m5sticks3/.pytest_cache/`, `gateway_m5sticks3/include/mesh_config.local.h`, `gateway_m5sticks3/README.local.md`, `__pycache__/`, `.pytest_cache/`, `.local-mesh-test/`, `log.txt`, `mesh-log.jsonl`, `mesh-outbox.json`, `mesh-egress.json`, `logs/`, `implants.json`, `sessions.json`, `index.html.gz`, `receiver.html.gz`.
- `installer.sh` inventories exact ports and MACs before interactive role assignment, requires `FLASH` confirmation, and invokes `flasher.py --port` so multiple attached boards cannot be selected implicitly.

## Work Guidance

- Keep behaviour changes mirrored in `README.md` (setup/user view) and `MESHOPS.md` (protocol/design view) when they alter setup, events, or topology semantics.
- Preserve the safety framing: no arbitrary payload/file transport, no HID when `mesh_only` is set, no secrets in examples.
- Prefer editing existing modules over adding new ones; add a test in `tests/` for every protocol, settings, store, or egress change.
- Keep example configs and `settings.json` schema in sync when adding a setting; validation must reject unsafe defaults.
- `ROADMAP.md` is the single backlog: update it when closing an item, confirming a new gap, or changing priority.

## Verification

- `python -m pytest -q` (CPython 3.12, 77+ tests). There is no `requirements.txt`, `pyproject.toml`, or `pytest.ini`; pytest is the only test dependency and tests run from the repo root.
- `python -m compileall -q main.py lib flasher.py`
- CI runs both in `.github/workflows/ci.yml`.
- `python flasher.py --port /dev/ttyACM0 --settings configs/node.example.json` flashes an exact board and needs an existing `mpremote` command + hardware; not CI. It validates settings first, skips USB packages for `mesh_only` and `lora_sink`, installs `urequests` only for `wifi` egress, and selects the role-specific compressed UI. `./installer.sh --inventory-only` is the non-flashing discovery check. `gateway_m5sticks3/` is an independent PlatformIO project; only its build/cache and local configuration are ignored.

## Child DOX Index

- `lib/AGENTS.md` — MicroPython runtime modules: ESP-NOW mesh networking, protocol, settings, storage, egress, API, HID/key logging.
- `tests/AGENTS.md` — CPython pytest suite and its hardware-free constraints.
- `configs/AGENTS.md` — canonical gateway/node/receiver example configurations and their required fields.
- `gateway_m5sticks3/AGENTS.md` — PlatformIO firmware for the optional M5StickS3 telemetry console and its private local configuration boundary.
