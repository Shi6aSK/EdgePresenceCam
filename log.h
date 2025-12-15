#pragma once

#include <Arduino.h>

void log_init();
void log_append(const char* fmt, ...);
String log_get_json();
