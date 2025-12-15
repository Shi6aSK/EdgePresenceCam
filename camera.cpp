#include "camera.h"
#include "presence.h"
#include "stats.h"
#include "log.h"

#include "esp_camera.h"
#define CAMERA_MODEL_XIAO_ESP32S3
#include "camera_pins.h"

static TaskHandle_t s_cameraTask = NULL;
static bool s_camera_connected = false;

static bool camera_hw_init() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  // Try a modest frame size for better reliability
  config.frame_size = FRAMESIZE_VGA;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("camera: esp_camera_init failed with error 0x%x\n", err);
    return false;
  }
  return true;
}

static void cameraTask(void* pv) {
  Serial.println("camera: task started, waiting for presence...");
  EventGroupHandle_t eg = presence_get_event_group();
  EventBits_t lastBits = 0;

  for (;;) {
    if (eg == NULL) {
      vTaskDelay(pdMS_TO_TICKS(200));
      continue;
    }

    // Wait up to 1s, check for presence bit
    EventBits_t bits = xEventGroupWaitBits(eg, PRESENCE_BIT, pdFALSE, pdFALSE, pdMS_TO_TICKS(1000));
    if ((bits & PRESENCE_BIT) && !(lastBits & PRESENCE_BIT)) {
      // Rising edge detected
      uint32_t t0 = millis();
      Serial.printf("camera: presence_detected @ %lu ms\n", (unsigned long)t0);
      log_append("camera presence_detected @ %lu", (unsigned long)t0);

      // Simulate capture delay
      vTaskDelay(pdMS_TO_TICKS(50));

      uint32_t t1 = millis();
      uint32_t delta = (t1 - t0);
      Serial.printf("camera: capture_done @ %lu ms (delta=%lu ms)\n", (unsigned long)t1, (unsigned long)delta);
      log_append("camera capture_done delta=%lu", (unsigned long)delta);
      // record latency
      stats_record_capture_latency(delta);
    }
    lastBits = bits;
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void camera_init() {
  if (s_cameraTask == NULL) {
    // Initialize camera hardware
    s_camera_connected = camera_hw_init();

    xTaskCreatePinnedToCore(cameraTask, "t_camera", 4096, NULL, 2, &s_cameraTask, 1);
    if (s_camera_connected) {
      Serial.println("camera: hardware initialized and task started");
      log_append("camera initialized");
    } else {
      Serial.println("camera: hardware not available; running simulation task");
      log_append("camera hw init failed; simulation mode");
    }
  }
}

void camera_trigger_manual() {
  presence_set(true);
}

bool camera_is_connected() {
  return s_camera_connected;
}
