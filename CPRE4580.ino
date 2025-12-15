#include <Arduino.h>
#include "radar.h"
#include "presence.h"
#include "camera.h"
#include "stats.h"
#include "log.h"
#include "web.h"
#include "modules/audio.h"
// The implementation is compiled separately by the Arduino build system.
// Note: pins and baud are passed to radar_init() directly to avoid
// duplicate-file-scope symbols.

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("CPRE4580: Booting and starting radar module...");

  // Initialize presence and camera subsystems
  presence_init();
  camera_init();
  // Initialize audio subsystem (LittleFS + simulated capture)
  audio_init();

  // Initialize radar module which starts its own FreeRTOS task
  // Use default board pins: D2 connected to sensor TX -> ESP RX,
  // and D3 connected to sensor RX <- ESP TX. Disable UART sniffer by default.
  radar_init(D2, D3, 256000);
  radar_set_sniffer(false);
  // start stats
  stats_init();
  // initialize logging and web server
  log_init();
  web_init();

  // Button task: simple poll-based button to trigger recordings
  // Default button pin; change if you have a dedicated button wired
  const int BUTTON_PIN = 0;
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  auto buttonTask = [](void* pv) {
    bool lastState = digitalRead(BUTTON_PIN);
    for (;;) {
      bool s = digitalRead(BUTTON_PIN);
      if (!s && lastState) {
        // button pressed (active low)
        Serial.println("button: pressed -> start 3000ms audio capture");
        log_append("button pressed -> audio capture");
        audio_start_recording(3000);
      }
      lastState = s;
      vTaskDelay(pdMS_TO_TICKS(50));
    }
  };
  xTaskCreatePinnedToCore(buttonTask, "t_button", 2048, NULL, 1, NULL, 1);
}

void loop() {
  // Idle; tasks run under FreeRTOS. Print a heartbeat every 5 seconds.
  static uint32_t last = 0;
  if (millis() - last > 5000) {
    last = millis();
    Serial.println("CPRE4580: main heartbeat");
  }
  vTaskDelay(pdMS_TO_TICKS(1000));
}
