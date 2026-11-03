#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <M5Unified.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <esp_idf_version.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include <cstring>

#include "mesh_config.local.h"

namespace {
constexpr uint8_t kProtocolVersion = 3;
constexpr size_t kMaxPacketBytes = 240;
constexpr size_t kMaxSourceLength = 31;
constexpr size_t kMaxKeyEventLength = 120;
constexpr size_t kDuplicateCapacity = 128;
constexpr uint32_t kRouteIntervalMs = 2000;
constexpr uint32_t kStatusIntervalMs = 5000;
constexpr uint32_t kNodeActiveWindowMs = 10000;
constexpr uint32_t kGatewayPollIntervalMs = 5000;
constexpr uint8_t kAckTtl = 6;
constexpr size_t kQueueLength = 16;

const uint8_t kBroadcastAddress[6] = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
const uint8_t kGatewayAddress[6] = GATEWAY_MAC;

struct ReceivedFrame {
  uint16_t length;
  uint8_t sourceMac[6];
  char payload[kMaxPacketBytes + 1];
};

struct SeenEvent {
  char source[kMaxSourceLength + 1];
  uint32_t sequence;
};

QueueHandle_t receivedQueue = nullptr;
SeenEvent seenEvents[kDuplicateCapacity] = {};
size_t seenWriteIndex = 0;
uint32_t routeSequence = 0;
uint32_t receivedCount = 0;
uint32_t droppedFrames = 0;
uint32_t lastRouteMs = 0;
uint32_t lastStatusMs = 0;
uint32_t lastGatewayPollMs = 0;
uint32_t nodeLastSeenMs[MESH_ALLOWED_NODE_COUNT] = {};
uint32_t nodeReceivedCount[MESH_ALLOWED_NODE_COUNT] = {};
bool storageReady = false;
bool gatewayOnline = false;
String gatewayStatus = "offline";

String formatMac(const uint8_t *mac) {
  char output[18];
  snprintf(output, sizeof(output), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(output);
}

bool isAllowedNodeMac(const uint8_t *mac) {
  if (mac == nullptr || (mac[0] & 0x01) != 0) {
    return false;
  }
  bool allZero = true;
  bool allBroadcast = true;
  for (size_t i = 0; i < 6; ++i) {
    allZero = allZero && mac[i] == 0x00;
    allBroadcast = allBroadcast && mac[i] == 0xff;
  }
  if (allZero || allBroadcast) {
    return false;
  }
  for (size_t peer = 0; peer < MESH_ALLOWED_NODE_COUNT; ++peer) {
    if (memcmp(mac, MESH_ALLOWED_NODE_MACS[peer], 6) == 0) {
      return true;
    }
  }
  return false;
}

void drawStatus(const char *title, const char *line1 = "",
                const char *line2 = "", const char *line3 = "") {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setTextSize(1);
  M5.Display.setCursor(4, 5);
  M5.Display.println("MeshOps / Gateway");
  M5.Display.drawFastHLine(4, 20, M5.Display.width() - 8, TFT_DARKGREY);
  M5.Display.setCursor(4, 28);
  M5.Display.println(title);
  M5.Display.setCursor(4, 54);
  M5.Display.println(line1);
  M5.Display.setCursor(4, 78);
  M5.Display.println(line2);
  M5.Display.setCursor(4, 102);
  M5.Display.println(line3);
  M5.Display.setCursor(4, 142);
  M5.Display.printf("RX: %lu  DROP: %lu", (unsigned long)receivedCount,
                    (unsigned long)droppedFrames);
  M5.Display.setCursor(4, 162);
  M5.Display.println(storageReady ? "Saved: /mesh-log.jsonl" : "Storage unavailable");
}

bool addBroadcastPeer() {
  if (esp_now_is_peer_exist(kBroadcastAddress)) {
    return true;
  }
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, kBroadcastAddress, sizeof(kBroadcastAddress));
  peer.channel = MESH_CHANNEL;
  peer.ifidx = WIFI_IF_STA;
  peer.encrypt = false;
  return esp_now_add_peer(&peer) == ESP_OK;
}

bool addEncryptedPeers() {
  if (strlen(MESH_PEER_KEY) != ESP_NOW_KEY_LEN ||
      esp_now_set_pmk(reinterpret_cast<const uint8_t *>(MESH_PEER_KEY)) != ESP_OK) {
    return false;
  }
  for (size_t index = 0; index < MESH_ALLOWED_NODE_COUNT; ++index) {
    if (esp_now_is_peer_exist(MESH_ALLOWED_NODE_MACS[index])) {
      continue;
    }
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, MESH_ALLOWED_NODE_MACS[index], 6);
    memcpy(peer.lmk, MESH_PEER_KEY, ESP_NOW_KEY_LEN);
    peer.channel = MESH_CHANNEL;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = true;
    if (esp_now_add_peer(&peer) != ESP_OK) {
      return false;
    }
  }
  if (!esp_now_is_peer_exist(kGatewayAddress)) {
    esp_now_peer_info_t gateway = {};
    memcpy(gateway.peer_addr, kGatewayAddress, 6);
    memcpy(gateway.lmk, MESH_PEER_KEY, ESP_NOW_KEY_LEN);
    gateway.channel = MESH_CHANNEL;
    gateway.ifidx = WIFI_IF_STA;
    gateway.encrypt = true;
    if (esp_now_add_peer(&gateway) != ESP_OK) {
      return false;
    }
  }
  return true;
}

bool forwardToGateway(const ReceivedFrame &frame) {
  const esp_err_t result = esp_now_send(
      kGatewayAddress, reinterpret_cast<const uint8_t *>(frame.payload),
      frame.length);
  if (result != ESP_OK) {
    ++droppedFrames;
    Serial.printf("DROP reason=gateway_forward result=%d\n", result);
    return false;
  }
  return true;
}

bool ensureGatewayWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }
  WiFi.begin(GATEWAY_MANAGEMENT_SSID, GATEWAY_MANAGEMENT_PASSWORD,
             MESH_CHANNEL);
  const uint32_t started = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - started < 4000) {
    delay(50);
  }
  return WiFi.status() == WL_CONNECTED;
}

bool gatewayRequest(const String &path, const String *body, String &response) {
  if (!ensureGatewayWiFi()) {
    gatewayOnline = false;
    gatewayStatus = "WiFi offline";
    return false;
  }
  HTTPClient http;
  http.begin(String(GATEWAY_MANAGEMENT_URL) + path);
  http.addHeader("Authorization", String("Bearer ") + GATEWAY_API_TOKEN);
  int status;
  if (body == nullptr) {
    status = http.GET();
  } else {
    http.addHeader("Content-Type", "application/json");
    status = http.POST(*body);
  }
  response = status > 0 ? http.getString() : String();
  http.end();
  gatewayOnline = status >= 200 && status < 300;
  gatewayStatus = gatewayOnline ? String("HTTP ") + status
                                : String("error ") + status;
  return gatewayOnline;
}

void pollGateway() {
  String response;
  if (gatewayRequest("/mesh/status", nullptr, response)) {
    JsonDocument status;
    if (deserializeJson(status, response) == DeserializationError::Ok) {
      const char *backend = status["egress"]["backend"] | "none";
      const int pending = status["egress"]["pending"] | 0;
      gatewayStatus = String(backend) + " q=" + pending;
    }
  }
  Serial.printf("GATEWAY online=%s status=%s\n",
                gatewayOnline ? "yes" : "no", gatewayStatus.c_str());
  lastGatewayPollMs = millis();
}

void advertiseGatewayRoute() {
  JsonDocument packet;
  packet["v"] = kProtocolVersion;
  packet["n"] = MESH_NETWORK_ID;
  packet["t"] = "r";
  packet["g"] = MESH_GATEWAY_ID;
  packet["s"] = MESH_GATEWAY_ID;
  packet["h"] = 0;
  packet["q"] = ++routeSequence;
  packet["p"] = "";

  char payload[kMaxPacketBytes + 1];
  const size_t length = serializeJson(packet, payload, sizeof(payload));
  if (length == 0 || length > kMaxPacketBytes || !addBroadcastPeer()) {
    ++droppedFrames;
    return;
  }
  if (esp_now_send(kBroadcastAddress,
                   reinterpret_cast<const uint8_t *>(payload), length) != ESP_OK) {
    ++droppedFrames;
  }
  lastRouteMs = millis();
}

bool isDuplicate(const char *source, uint32_t sequence) {
  for (const SeenEvent &event : seenEvents) {
    if (event.sequence == sequence && strcmp(event.source, source) == 0) {
      return true;
    }
  }
  SeenEvent &slot = seenEvents[seenWriteIndex];
  const size_t sourceLength = strlen(source);
  memcpy(slot.source, source, sourceLength + 1);
  slot.sequence = sequence;
  seenWriteIndex = (seenWriteIndex + 1) % kDuplicateCapacity;
  return false;
}

bool isAllowedKind(const char *kind) {
  return strcmp(kind, "heartbeat") == 0 ||
         strcmp(kind, "synthetic_counter") == 0 ||
         strcmp(kind, "link_test") == 0 || strcmp(kind, "key_event") == 0;
}

bool isValidKeyEvent(const char *value) {
  const size_t length = strlen(value);
  if (length == 0 || length > kMaxKeyEventLength) {
    return false;
  }
  for (size_t index = 0; index < length; ++index) {
    if (static_cast<uint8_t>(value[index]) < 0x20) {
      return false;
    }
  }
  return true;
}

void storeEvent(const char *source, uint32_t sequence, const char *kind,
                JsonVariantConst value, uint32_t hops,
                const uint8_t *lastHopMac) {
  if (!storageReady) {
    return;
  }
  File output = LittleFS.open(MESH_LOG_PATH, FILE_APPEND);
  if (!output) {
    storageReady = false;
    return;
  }
  JsonDocument record;
  record["received_ms"] = millis();
  record["source"] = source;
  record["sequence"] = sequence;
  record["kind"] = kind;
  record["value"] = value;
  record["hops"] = hops;
  record["last_hop_mac"] = formatMac(lastHopMac);
  serializeJson(record, output);
  output.println();
  output.close();
}

void sendAck(const uint8_t *destination, const char *source,
             uint32_t sequence) {
  JsonDocument packet;
  packet["v"] = kProtocolVersion;
  packet["n"] = MESH_NETWORK_ID;
  packet["t"] = "a";
  packet["s"] = source;
  packet["q"] = sequence;
  packet["l"] = kAckTtl;

  char payload[kMaxPacketBytes + 1];
  const size_t length = serializeJson(packet, payload, sizeof(payload));
  if (length == 0 || length > kMaxPacketBytes ||
      esp_now_send(destination, reinterpret_cast<const uint8_t *>(payload),
                   length) != ESP_OK) {
    ++droppedFrames;
    Serial.printf("DROP reason=ack_send source=%s seq=%lu via=%s\n", source,
                  (unsigned long)sequence, formatMac(destination).c_str());
  }
}

void markNodeReceived(const char *source) {
  for (size_t index = 0; index < MESH_ALLOWED_NODE_COUNT; ++index) {
    if (strcmp(source, MESH_ALLOWED_NODE_IDS[index]) == 0) {
      nodeLastSeenMs[index] = millis();
      ++nodeReceivedCount[index];
      return;
    }
  }
}

void printStatus() {
  const uint32_t now = millis();
  Serial.printf("STATUS rx=%lu drop=%lu active=[",
                (unsigned long)receivedCount, (unsigned long)droppedFrames);
  bool first = true;
  for (size_t index = 0; index < MESH_ALLOWED_NODE_COUNT; ++index) {
    if (nodeLastSeenMs[index] != 0 &&
        now - nodeLastSeenMs[index] <= kNodeActiveWindowMs) {
      Serial.printf("%s%s(rx=%lu,age_ms=%lu)", first ? "" : ",",
                    MESH_ALLOWED_NODE_IDS[index],
                    (unsigned long)nodeReceivedCount[index],
                    (unsigned long)(now - nodeLastSeenMs[index]));
      first = false;
    }
  }
  Serial.println("]");
  lastStatusMs = now;
}

void processFrame(const ReceivedFrame &frame) {
  JsonDocument packet;
  const DeserializationError error =
      deserializeJson(packet, frame.payload, frame.length);
  if (error || packet["v"].as<uint8_t>() != kProtocolVersion ||
      packet["n"] != MESH_NETWORK_ID || packet["t"] != "d") {
    ++droppedFrames;
    Serial.printf("DROP reason=decode_or_header via=%s\n",
                  formatMac(frame.sourceMac).c_str());
    return;
  }

  const char *source = packet["s"] | "";
  const char *kind = packet["k"] | "";
  const bool keyEvent = strcmp(kind, "key_event") == 0;
  const char *keyValue = keyEvent ? (packet["x"] | "") : "";
  if (source[0] == '\0' || strlen(source) > kMaxSourceLength ||
      !isAllowedKind(kind) || !packet["q"].is<uint32_t>() ||
      !packet["l"].is<uint32_t>() || !packet["h"].is<uint32_t>() ||
      (keyEvent && !isValidKeyEvent(keyValue)) ||
      (!keyEvent && !packet["x"].is<int32_t>())) {
    ++droppedFrames;
    Serial.printf("DROP reason=invalid_data via=%s\n",
                  formatMac(frame.sourceMac).c_str());
    return;
  }

  const uint32_t sequence = packet["q"].as<uint32_t>();
  if (isDuplicate(source, sequence)) {
    forwardToGateway(frame);
    sendAck(frame.sourceMac, source, sequence);
    return;
  }

  ++receivedCount;
  markNodeReceived(source);
  const uint32_t hops = packet["h"].as<uint32_t>();
  const String value = packet["x"].as<String>();
  storeEvent(source, sequence, kind, packet["x"], hops, frame.sourceMac);
  forwardToGateway(frame);
  sendAck(frame.sourceMac, source, sequence);
  Serial.printf(
      "EVENT source=%s kind=%s value=%s seq=%lu hops=%lu via=%s\n", source,
      kind, value.c_str(), (unsigned long)sequence, (unsigned long)hops,
      formatMac(frame.sourceMac).c_str());
  drawStatus("Telemetry received", source, kind,
             (String("value: ") + value + "  seq: " +
              String(sequence)).c_str());
}

#if ESP_IDF_VERSION_MAJOR >= 5
void receiveCallback(const esp_now_recv_info_t *info, const uint8_t *data,
                     int length) {
  const uint8_t *sourceMac = info == nullptr ? nullptr : info->src_addr;
#else
void receiveCallback(const uint8_t *sourceMac, const uint8_t *data,
                     int length) {
#endif
  if (!isAllowedNodeMac(sourceMac) ||
      receivedQueue == nullptr || data == nullptr || length <= 0 ||
      length > static_cast<int>(kMaxPacketBytes)) {
    ++droppedFrames;
    return;
  }
  ReceivedFrame frame = {};
  frame.length = static_cast<uint16_t>(length);
  memcpy(frame.sourceMac, sourceMac, sizeof(frame.sourceMac));
  memcpy(frame.payload, data, length);
  frame.payload[length] = '\0';
  if (xQueueSend(receivedQueue, &frame, 0) != pdTRUE) {
    ++droppedFrames;
  }
}
}  // namespace

void setup() {
  Serial.begin(115200);
  auto m5Config = M5.config();
  M5.begin(m5Config);
  delay(100);
  M5.Display.setRotation(0);
  drawStatus("Starting", MESH_NETWORK_ID, "channel setup", "synthetic data only");

  // A freshly flashed board may not have a LittleFS filesystem yet. Mount the
  // existing filesystem when possible and format only when mounting fails.
  storageReady = LittleFS.begin(true);
  Serial.printf("MeshOps storage=%s path=%s\n",
                storageReady ? "ready" : "unavailable", MESH_LOG_PATH);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  Serial.printf("MeshOps gateway id=%s mac=%s channel=%u\n",
                MESH_GATEWAY_ID, WiFi.macAddress().c_str(), MESH_CHANNEL);
  if (esp_wifi_set_channel(MESH_CHANNEL, WIFI_SECOND_CHAN_NONE) != ESP_OK) {
    drawStatus("Wi-Fi channel error", "Check mesh_config.local.h");
    return;
  }

  receivedQueue = xQueueCreate(kQueueLength, sizeof(ReceivedFrame));
  if (receivedQueue == nullptr || esp_now_init() != ESP_OK ||
      !addBroadcastPeer() || !addEncryptedPeers()) {
    drawStatus("ESP-NOW startup failed");
    return;
  }

  esp_now_register_recv_cb(receiveCallback);
  advertiseGatewayRoute();
  drawStatus("Waiting for nodes", MESH_NETWORK_ID,
             WiFi.macAddress().c_str(),
             (String("channel: ") + String(MESH_CHANNEL)).c_str());
  ensureGatewayWiFi();
  pollGateway();
}

void loop() {
  M5.update();
  if (M5.BtnA.wasPressed()) {
    pollGateway();
    drawStatus("Gateway status", gatewayStatus.c_str(),
               gatewayOnline ? "online" : "offline", "egress is manual");
  }
  if (M5.BtnB.wasPressed()) {
    drawStatus("Manual config only", "default egress: LoRa",
               "use gateway settings", gatewayStatus.c_str());
  }
  if (millis() - lastRouteMs >= kRouteIntervalMs) {
    advertiseGatewayRoute();
  }
  if (millis() - lastStatusMs >= kStatusIntervalMs) {
    printStatus();
  }
  if (millis() - lastGatewayPollMs >= kGatewayPollIntervalMs) {
    pollGateway();
  }

  ReceivedFrame frame;
  if (receivedQueue != nullptr &&
      xQueueReceive(receivedQueue, &frame, pdMS_TO_TICKS(20)) == pdTRUE) {
    processFrame(frame);
  } else {
    delay(5);
  }
}
