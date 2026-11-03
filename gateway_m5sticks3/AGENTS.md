# gateway_m5sticks3/

## Purpose

- PlatformIO firmware for the optional M5StickS3 telemetry console.
- Observes authorized MeshOps events, forwards validated events to the real gateway, and displays gateway health.

## Ownership

- Owns the Arduino/PlatformIO console firmware and its safe configuration template.
- Gateway runtime, egress, and authenticated configuration remain owned by the root MicroPython project.

## Local Contracts

- `include/mesh_config.example.h` contains placeholders and locally administered example MACs only.
- `include/mesh_config.local.h` contains real secrets/MACs, is ignored by Git, and must never be shared.
- The console may poll gateway status but must not change egress; deployed gateway egress defaults to LoRa and Wi-Fi is configured manually by an operator.
- Accepted ESP-NOW packets must match the protocol version, network ID, event allowlist, peer MAC allowlist, and bounded field lengths.

## Work Guidance

- Keep secrets and real device identifiers out of source, docs, serial examples, and tests.
- Preserve the authorization warning and do not add arbitrary payload or remote HID capabilities.

## Verification

- `pio run -d gateway_m5sticks3 -e m5stack-sticks3` with a private `mesh_config.local.h`.
- Hardware upload uses `pio run -d gateway_m5sticks3 -e m5stack-sticks3 -t upload --upload-port <port>`.

## Child DOX Index

- None.
