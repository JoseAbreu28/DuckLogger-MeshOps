# configs/

## Purpose

- Canonical example device configurations copied to a board as `settings.json`.

## Ownership

- Owns the documented config schema examples. Parsing/validation lives in `lib/settings.py`.

## Local Contracts

- `gateway.example.json`: ESP-NOW gateway with native authenticated SX1262 egress, management AP, bounded spool, retry policy, and radio pin/frequency settings.
- `node.example.json`: `device.role: node`, `mesh_only: true`, `data_source: true`, `synthetic_telemetry: false`, management disabled, mesh block only.
- `receiver.example.json`: external `lora_sink` with SX1262 receive/ACK, management AP, authenticated web console, and rotating log retention.
- `mesh.transport` is `espnow`; unsupported values are rejected. `mesh.peer_macs` is required and maps peer device id to MAC; `mesh.peer_key` must be 16 bytes. The two examples list each other as reciprocal peers.
- Secrets stay as `CHANGE_ME_*` placeholders so startup validation fails until they are replaced. Never commit real credentials.
- Shared values across examples (`network_id`, `gateway_id`, `channel`, keys) must stay mutually consistent.
- Gateway and receiver must use identical `lora` modulation values and key. Example secrets are placeholders and must fail validation.
- Gateway and receiver examples target the Seeed 113110064 XIAO ESP32S3 + Wio-SX1262 B2B kit: SPI GPIO 7/8/9, NSS 41, RESET 42, BUSY 40, DIO1 39, and RF switch 38.

## Work Guidance

- When `lib/settings.py` gains a setting, add it to the relevant example with a safe default and document it in `README.md` / `MESHOPS.md`.
- Keep gateway and receiver `lora` blocks structurally parallel; keep node and gateway mesh blocks aligned where fields are shared.

## Verification

- `python -m pytest -q tests/test_settings.py` exercises default and validation paths.
- Loading an example as-is must still fail validation on placeholders; that is expected.

## Child DOX Index

- None.
