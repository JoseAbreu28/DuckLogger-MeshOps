# tests/

## Purpose

- CPython pytest suite that exercises hardware-free modules from `lib/` without an ESP32 attached.

## Ownership

- Owns test code and fixtures only. Production behaviour changes belong in `lib/` or `main.py`.

## Local Contracts

- `conftest.py` only prepends `lib/` to `sys.path`; import modules by bare name (`import mesh_protocol`). It does not stub hardware.
- Tests run on CPython 3.12 (CI) and must not require real hardware, network, USB, or UART.
- `test_mesh_network.py` injects a fake `RadioTransport`; hardware adapters keep device imports lazy and are not required for core tests.
- Pure logic is tested directly: `mesh_protocol`, `settings`, `mesh_store`, `egress`, LoRa framing/link/sink, keyframe/logger decoding, API with stub mesh/radio.
- Assert on documented contracts: event allowlist, validation errors, auth requirements, bounded queues, NDJSON output.
- `pytest.ini`/config is intentionally absent; run from the repo root.

## Work Guidance

- Add or extend a test with every behaviour change in `lib/`; name files `test_<module>.py`.
- Prefer explicit fake/stub objects over `unittest.mock` magic for MicroPython-shaped modules.
- Keep tests deterministic: no sleeps, no real wall-clock dependence, no fixed ports.

## Verification

- `python -m pytest -q` from the repo root (must stay green).
- `python -m compileall -q tests` if adding syntax-sensitive code.

## Child DOX Index

- None.
