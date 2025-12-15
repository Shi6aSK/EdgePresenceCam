#include "radar.h"
#include "presence.h"
#include "log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// Simple radar parser module for LD2410-like mmWave sensor
// Exposes radar_init(rxPin, txPin, baud) which starts a FreeRTOS task

static TaskHandle_t tRadarHandle = nullptr;
static int RADAR_RX_PIN = -1;
static int RADAR_TX_PIN = -1;
static uint32_t RADAR_BAUD = 256000;

// Last decoded state (shared between radar task and external getters)
static uint8_t s_lastState = 0xFF;

// LD2410 basic report frame markers
static const uint8_t HDR[4]  = {0xF4, 0xF3, 0xF2, 0xF1};
static const uint8_t TAIL[4] = {0xF8, 0xF7, 0xF6, 0xF5};

static uint8_t frame[256];
// Last raw frame snapshot for debugging
static uint8_t s_lastFrameBuf[256];
static size_t s_lastFrameLen = 0;
// Legacy info string (for frames that start with 0x53,0x59)
static String s_lastLegacyInfo = String("");
// Optional raw UART sniffer
static volatile bool s_sniffer = false;

// Transfer queue and debug/transfer state
typedef struct {
  uint32_t ts_ms;
  uint8_t state;
  uint16_t frame_len;
} RadarEvent_t;

static QueueHandle_t s_eventQueue = NULL;
static TaskHandle_t s_transferTask = NULL;
static volatile bool s_transferring = false;
static uint32_t s_lastTransferTs = 0;
static uint8_t s_lastBufferedState = 0xFF;
// Recent history circular buffer for UI plotting
struct HistoryItem { uint32_t ts_ms; uint8_t state; };
static HistoryItem s_history[128];
static size_t s_history_head = 0;
static size_t s_history_count = 0;

static bool findHeader(const uint8_t* b, size_t i) {
  return b[i]==HDR[0] && b[i+1]==HDR[1] && b[i+2]==HDR[2] && b[i+3]==HDR[3];
}
static bool findTail(const uint8_t* b, size_t i) {
  return b[i]==TAIL[0] && b[i+1]==TAIL[1] && b[i+2]==TAIL[2] && b[i+3]==TAIL[3];
}

static bool decodeTargetState(const uint8_t* f, size_t n, uint8_t &stateOut) {
  if (n < 4 + 2 + 1 + 1 + 1 + 4) return false;
  uint8_t reportType = f[6];
  if (reportType != 0x02 && reportType != 0x01) return false;
  if (f[7] != 0xAA) return false;
  stateOut = f[8];
  return true;
}

static const char* stateName(uint8_t s) {
  switch (s) {
    case 0x00: return "NO_TARGET";
    case 0x01: return "MOVING";
    case 0x02: return "STATIONARY";
    case 0x03: return "MOVING+STATIONARY";
    default:
      if (s == 0xFF) return "NO_DATA";
      return "UNKNOWN";
  }
}

static void radarTask(void* pv) {
  Serial.println("Radar task started.");

  size_t idx = 0;
  uint32_t lastPrintMs = 0;

  for (;;) {
    while (Serial1.available()) {
      uint8_t b = (uint8_t)Serial1.read();
      if (s_sniffer) {
        Serial.printf("%02X ", b);
      }
      frame[idx++] = b;
      if (idx >= sizeof(frame)) idx = 0;
      if (idx >= 8 && findTail(frame, (idx - 4) % sizeof(frame))) {
        // Find header within buffer
        size_t start = 0;
        bool found = false;
        for (size_t i = 0; i + 4 < idx; i++) {
          if (findHeader(frame, i)) { start = i; found = true; break; }
        }
        if (found) {
          size_t len = idx - start;
          // snapshot raw frame for debugging
          size_t copyLen = (len <= sizeof(s_lastFrameBuf)) ? len : sizeof(s_lastFrameBuf);
          memcpy(s_lastFrameBuf, frame + start, copyLen);
          s_lastFrameLen = copyLen;
          // If frame looks like legacy MR24-style (starts with 0x53,0x59), store a small summary
          if (s_lastFrameLen >= 2 && s_lastFrameBuf[0] == 0x53 && s_lastFrameBuf[1] == 0x59) {
            String info = "legacy frame: ";
            // show first 12 bytes as hex and also some decimal fields for quick inspection
            for (size_t ii = 0; ii < s_lastFrameLen && ii < 16; ++ii) {
              char tmp[8];
              snprintf(tmp, sizeof(tmp), "%02X", s_lastFrameBuf[ii]);
              if (ii) info += ' ';
              info += String(tmp);
            }
            // add brief decimal view for bytes 2..6 if present
            if (s_lastFrameLen > 6) {
              info += " | dec:";
              for (size_t ii = 2; ii <= 6 && ii < s_lastFrameLen; ++ii) {
                info += String((unsigned int)s_lastFrameBuf[ii]);
                if (ii < 6 && ii+1 < s_lastFrameLen) info += ',';
              }
            }
            s_lastLegacyInfo = info;
            log_append("RADAR legacy_info: %s", info.c_str());
          }
          // Debug checkpoint: header located
          log_append("RADAR debug: header_found at %u", (unsigned int)start);
          uint8_t state;
          if (decodeTargetState(frame + start, len, state)) {
            if (state != s_lastState || (millis() - lastPrintMs) > 1000) {
              s_lastState = state;
              lastPrintMs = millis();
              Serial.printf("[RADAR] target=0x%02X (%s)\n", state, stateName(state));
              log_append("RADAR target=0x%02X (%s)", state, stateName(state));
            }
            // Debug checkpoint: decoded successfully
            log_append("RADAR debug: decoded state=0x%02X len=%u", state, (unsigned int)len);

            // Ensure queue exists
            if (s_eventQueue == NULL) {
              s_eventQueue = xQueueCreate(8, sizeof(RadarEvent_t));
            }
            // Enqueue event for transfer/processing
            RadarEvent_t ev;
            ev.ts_ms = millis();
            ev.state = state;
            ev.frame_len = (uint16_t)len;
            // Record to history (circular)
            s_history[s_history_head] = { ev.ts_ms, ev.state };
            s_history_head = (s_history_head + 1) % (sizeof(s_history)/sizeof(s_history[0]));
            if (s_history_count < (sizeof(s_history)/sizeof(s_history[0]))) s_history_count++;

            // Only enqueue when state changed or sufficient time elapsed since last transfer
            if (s_eventQueue) {
              uint32_t now = ev.ts_ms;
              if (state != s_lastBufferedState || (now - s_lastTransferTs) > 2000) {
                if (xQueueSend(s_eventQueue, &ev, 0) == pdTRUE) {
                  s_lastBufferedState = state;
                  log_append("RADAR debug: queued state=0x%02X ts=%lu", state, (unsigned long)ev.ts_ms);
                } else {
                  log_append("RADAR debug: queue_full state=0x%02X", state);
                }
              }
            }

            // Map non-zero state to presence ON
            if (state == 0x00) {
              presence_set(false);
            } else {
              presence_set(true);
            }
          } else {
            if ((millis() - lastPrintMs) > 2000) {
              lastPrintMs = millis();
              Serial.println("[RADAR] Frame seen but decode failed (check pins/baud).");
              log_append("RADAR decode_failed");
            }
          }
        }
        idx = 0;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

void radar_set_sniffer(bool on) {
  s_sniffer = on;
  if (on) log_append("RADAR: UART sniffer enabled");
  else log_append("RADAR: UART sniffer disabled");
}

bool radar_get_sniffer() {
  return s_sniffer;
}

void radar_init(int rxPin, int txPin, uint32_t baud) {
  RADAR_RX_PIN = rxPin;
  RADAR_TX_PIN = txPin;
  RADAR_BAUD = baud;

  Serial1.begin(RADAR_BAUD, SERIAL_8N1, RADAR_RX_PIN, RADAR_TX_PIN);
  Serial.printf("Serial1 started at %lu (rx=%d tx=%d)\n", (unsigned long)RADAR_BAUD, RADAR_RX_PIN, RADAR_TX_PIN);

  if (tRadarHandle == nullptr) {
    xTaskCreatePinnedToCore(
      radarTask, "t_radar", 4096, nullptr, 3, &tRadarHandle, 1
    );
  }

  // Create transfer worker which consumes queued radar events and simulates
  // transfer. Replace simulated transfer with real network/upload logic.
  if (s_transferTask == NULL) {
    if (s_eventQueue == NULL) {
      s_eventQueue = xQueueCreate(8, sizeof(RadarEvent_t));
    }
    auto transferWorker = [](void* pv) {
      RadarEvent_t ev;
      for (;;) {
        if (s_eventQueue == NULL) { vTaskDelay(pdMS_TO_TICKS(200)); continue; }
          if (xQueueReceive(s_eventQueue, &ev, portMAX_DELAY) == pdTRUE) {
          s_transferring = true;
          s_lastTransferTs = ev.ts_ms;
          log_append("RADAR transfer start state=0x%02X ts=%lu", ev.state, (unsigned long)ev.ts_ms);
          // NOTE: avoid noisy Serial prints here; keep log_append for persistent debug
          // Simulate transfer latency
          vTaskDelay(pdMS_TO_TICKS(150));
          log_append("RADAR transfer end state=0x%02X ts=%lu", ev.state, (unsigned long)millis());
          s_transferring = false;
        }
      }
    };
    xTaskCreatePinnedToCore(transferWorker, "t_radar_xfer", 4096, NULL, 2, &s_transferTask, 1);
  }
}

// Expose last decoded state to other modules
uint8_t radar_get_last_state() {
  return s_lastState;
}

const char* radar_get_last_state_name() {
  return stateName(s_lastState);
}

bool radar_is_transferring() {
  return s_transferring;
}

int radar_get_queue_size() {
  if (s_eventQueue == NULL) return 0;
  return (int)uxQueueMessagesWaiting(s_eventQueue);
}

// Return recent history as JSON array string (caller should send as application/json)
String radar_get_history_json() {
  String out = "[";
  size_t n = s_history_count;
  size_t start = (s_history_head + (sizeof(s_history)/sizeof(s_history[0])) - n) % (sizeof(s_history)/sizeof(s_history[0]));
  for (size_t i = 0; i < n; ++i) {
    size_t idx = (start + i) % (sizeof(s_history)/sizeof(s_history[0]));
    if (i) out += ",";
    out += "{";
    out += "\"ts\":" + String(s_history[idx].ts_ms);
    out += ",\"state\":" + String(s_history[idx].state);
    out += ",\"name\":\"" + String(stateName(s_history[idx].state)) + "\"";
    out += "}";
  }
  out += "]";
  return out;
}

String radar_get_last_legacy_info() {
  return s_lastLegacyInfo;
}

String radar_get_last_frame_hex() {
  if (s_lastFrameLen == 0) return String("");
  String out;
  out.reserve(s_lastFrameLen * 3 + 1);
  for (size_t i = 0; i < s_lastFrameLen; ++i) {
    char buf[4];
    snprintf(buf, sizeof(buf), "%02X", s_lastFrameBuf[i]);
    if (i) out += ' ';
    out += buf;
  }
  return out;
}

int radar_get_serial_available() {
  return Serial1.available();
}
