#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

// Presence EventGroup API
void presence_init();
void presence_set(bool on);
EventGroupHandle_t presence_get_event_group();

// Bit definitions
static const EventBits_t PRESENCE_BIT = (1 << 0);
