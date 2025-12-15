#pragma once

#include <Arduino.h>

// Initialize the radar parser task. Call once from setup().
void radar_init(int rxPin, int txPin, uint32_t baud);

// Query the last-decoded radar state byte (0x00 = NO_TARGET, etc.)
uint8_t radar_get_last_state();
// Human-readable name for the last decoded state
const char* radar_get_last_state_name();

// Transfer/queue status helpers
// Returns true if a transfer is currently active
bool radar_is_transferring();
// Returns number of queued radar events waiting transfer
int radar_get_queue_size();

// Debug: return last raw frame snapshot as a hex string
String radar_get_last_frame_hex();
// Debug: return the number of bytes currently available on Serial1
int radar_get_serial_available();

// Return recent history as JSON array string for UI plotting
String radar_get_history_json();
// Return a short summary string for legacy frames (or empty)
String radar_get_last_legacy_info();

// Enable/disable a raw-hex Serial1 sniffer for diagnostics
void radar_set_sniffer(bool on);
bool radar_get_sniffer();
