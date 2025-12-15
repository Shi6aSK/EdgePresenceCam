#pragma once

/*
  Bluetooth module skeleton

  Responsibilities:
  - Scan for nearby Bluetooth devices (GAP)
  - Connect to a selected device (A2DP Sink or Classic) and stream audio
  - Expose simple APIs for scan, list, connect, disconnect
  - Provide hooks for the web UI to request scans and show results

  TODOs / Requirements:
  - [ ] Implement `bt_init()`, `bt_scan()`, `bt_get_devices()` and `bt_connect(addr)`
  - [ ] Use ESP32 A2DP Source (or appropriate profile) implementation; might require ESP-IDF
  - [ ] Provide connection status callbacks to the manager and web UI
*/

#include <Arduino.h>

void bt_init();
void bt_scan();
String bt_get_devices_json();
bool bt_connect(const char* addr);
void bt_disconnect();
