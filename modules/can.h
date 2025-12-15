#pragma once

/*
  CAN (TWAI) module skeleton

  Responsibilities:
  - Initialize TWAI (ESP32 CAN) interface for simulated CAN bus
  - Send/receive demo frames triggered by a GPIO button
  - Provide APIs to send frames and register callbacks for received frames

  TODOs / Requirements:
  - [ ] Implement `can_init()` to configure TWAI and start tasks
  - [ ] Implement `can_send_demo_frame()` for button-triggered demo
  - [ ] Provide `can_register_rx_callback(fn)` to handle incoming messages
*/

#include <Arduino.h>

using can_rx_cb_t = void(*)(uint32_t id, const uint8_t* data, size_t len);

void can_init();
void can_send_demo_frame();
void can_register_rx_callback(can_rx_cb_t cb);
