#pragma once

#include <Arduino.h>

void stats_init();
void stats_record_presence_update(bool present);
void stats_record_capture_latency(uint32_t ms);

// For external reads (if needed)
uint32_t stats_get_capture_count();
uint32_t stats_get_last_presence_percent();
