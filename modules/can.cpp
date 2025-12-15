#include "can.h"

/*
  TODOs / Implementation notes:
  - Use esp-idf TWAI APIs or Arduino wrapper if available for ESP32-S3.
  - For the demo: one task sends a periodic frame when a button is pressed; a loopback receive toggles an LED.
  - Implement basic filtering and acknowledgement for deterministic response testing.
*/

static can_rx_cb_t s_rx_cb = nullptr;
static bool s_inited = false;

void can_init() {
  if (s_inited) return;
  // TODO: configure TWAI pins, bitrate, start driver
  s_inited = true;
}

void can_send_demo_frame() {
  // TODO: construct a TWAI frame and send it
}

void can_register_rx_callback(can_rx_cb_t cb) {
  s_rx_cb = cb;
}
