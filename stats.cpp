#include "stats.h"
#include "log.h"

static TaskHandle_t s_statsTask = NULL;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;

// Presence timing
static bool s_presence_state = false;
static uint32_t s_presence_start_ms = 0;
static uint32_t s_presence_accum_ms = 0; // accumulated ms within current reporting window
static uint32_t s_last_presence_percent = 0;

// Capture latencies
static uint32_t s_capture_count = 0;
static uint32_t s_capture_min_ms = UINT32_MAX;
static uint32_t s_capture_max_ms = 0;

void stats_record_presence_update(bool present) {
  portENTER_CRITICAL(&s_lock);
  uint32_t now = millis();
  if (present && !s_presence_state) {
    // rising
    s_presence_state = true;
    s_presence_start_ms = now;
  } else if (!present && s_presence_state) {
    // falling
    if (now > s_presence_start_ms) {
      s_presence_accum_ms += (now - s_presence_start_ms);
    }
    s_presence_state = false;
    s_presence_start_ms = 0;
  }
  portEXIT_CRITICAL(&s_lock);
}

void stats_record_capture_latency(uint32_t ms) {
  portENTER_CRITICAL(&s_lock);
  s_capture_count++;
  if (ms < s_capture_min_ms) s_capture_min_ms = ms;
  if (ms > s_capture_max_ms) s_capture_max_ms = ms;
  portEXIT_CRITICAL(&s_lock);
}

static void statsTask(void* pv) {
  Serial.println("stats: task started");
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    portENTER_CRITICAL(&s_lock);
    uint32_t now = millis();
    uint32_t windowAccum = s_presence_accum_ms;
    if (s_presence_state && now > s_presence_start_ms) {
      windowAccum += (now - s_presence_start_ms);
      // keep start_ms as-is (continuing presence)
      s_presence_start_ms = now; // reset to count only new ms next window
    }
    // compute percent within 1s window
    uint32_t percent = (windowAccum >= 1000) ? 100 : (windowAccum * 100) / 1000;
    s_last_presence_percent = percent;

    // snapshot capture stats
    uint32_t capCount = s_capture_count;
    uint32_t capMin = (s_capture_min_ms==UINT32_MAX) ? 0 : s_capture_min_ms;
    uint32_t capMax = s_capture_max_ms;

    // reset window accumulators
    s_presence_accum_ms = 0;
    s_capture_count = 0;
    s_capture_min_ms = UINT32_MAX;
    s_capture_max_ms = 0;
    portEXIT_CRITICAL(&s_lock);

    // print a tidy line
    Serial.printf("stats: presence%%=%u capture_count=%u min=%lums max=%lums\n",
            (unsigned)percent, (unsigned)capCount, (unsigned long)capMin, (unsigned long)capMax);
    log_append("stats pct=%u cnt=%u min=%lu max=%lu", (unsigned)percent, (unsigned)capCount, (unsigned long)capMin, (unsigned long)capMax);
  }
}

void stats_init() {
  if (s_statsTask == NULL) {
    xTaskCreatePinnedToCore(statsTask, "t_stats", 4096, NULL, 1, &s_statsTask, 1);
  }
}

uint32_t stats_get_capture_count() { return s_capture_count; }
uint32_t stats_get_last_presence_percent() { return s_last_presence_percent; }
