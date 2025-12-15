#include "bluetooth.h"

/*
  TODOs / Implementation notes:
  - Arduino-ESP32 has some Bluetooth examples (BLE and Classic). For A2DP streaming, consider esp-idf components or an A2DP library.
  - Keep the API simple: scan -> returns JSON list; connect(addr) -> attempts to pair/connect and returns success.
*/

static bool s_inited = false;

void bt_init() {
  if (s_inited) return;
  // TODO: initialize BT controller, bluedroid stack, etc.
  s_inited = true;
}

void bt_scan() {
  // TODO: start a scan and populate internal device list
}

String bt_get_devices_json() {
  // TODO: return a JSON array of discovered devices (id/address/name)
  return String("[]");
}

bool bt_connect(const char* addr) {
  // TODO: connect and return connection state
  (void)addr;
  return false;
}

void bt_disconnect() {
  // TODO: disconnect if connected
}
