#pragma once

#include <Arduino.h>

// Start the camera trigger task. The task watches the presence EventGroup and
// performs a (simulated) capture on rising edges.
void camera_init();

// For testing: request a manual capture
void camera_trigger_manual();

// Returns true if a camera device was successfully initialized/connected.
bool camera_is_connected();
