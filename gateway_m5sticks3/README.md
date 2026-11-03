# M5StickS3 telemetry console

This independent PlatformIO project builds the optional telemetry console.
It observes authorized MeshOps events, forwards validated events to the real
gateway, and displays gateway health. It does not select gateway egress; LoRa
is the deployed default and Wi-Fi egress remains a manual operator setting.

Before building, copy the safe template and replace all placeholders locally:

```bash
cp include/mesh_config.example.h include/mesh_config.local.h
pio run -e m5stack-sticks3
```

`mesh_config.local.h` contains the real peer key, management credentials, and
MAC allowlist. It is ignored by Git and must never be committed or shared.
