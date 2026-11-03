#pragma once

// Copy this file to mesh_config.local.h and replace every CHANGE_ME value.
// Keep mesh_config.local.h private; it is excluded by .gitignore.
#define MESH_NETWORK_ID "lab-mesh"
#define MESH_GATEWAY_ID "gateway-01"
#define MESH_PEER_KEY "CHANGE_ME_16BYTE"
#define MESH_CHANNEL 6
#define MESH_LOG_PATH "/mesh-log.jsonl"

// Locally administered example addresses; replace them with device MACs.
#define GATEWAY_MAC {0x02, 0x00, 0x00, 0x00, 0x00, 0x01}
#define MESH_ALLOWED_NODE_COUNT 2
static const char *MESH_ALLOWED_NODE_IDS[MESH_ALLOWED_NODE_COUNT] = {
    "implant-a", "implant-b"};
static const uint8_t MESH_ALLOWED_NODE_MACS[MESH_ALLOWED_NODE_COUNT][6] = {
    {0x02, 0x00, 0x00, 0x00, 0x00, 0x02},
    {0x02, 0x00, 0x00, 0x00, 0x00, 0x03},
};

#define GATEWAY_MANAGEMENT_SSID "meshops-gateway"
#define GATEWAY_MANAGEMENT_PASSWORD "CHANGE_ME_PASSWORD"
#define GATEWAY_API_TOKEN "CHANGE_ME_TOKEN_32_CHARS"
#define GATEWAY_MANAGEMENT_URL "http://192.168.4.1"
