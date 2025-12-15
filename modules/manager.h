#pragma once

/*
  Manager / Orchestrator skeleton

  Responsibilities:
  - Initialize and coordinate all modules (radar/presence/camera/audio/cloud/tts/bt/can/web)
  - Define FreeRTOS task priorities and create high-level tasks (audio worker, cloud worker)
  - Handle events (presence -> camera trigger -> audio record -> cloud upload -> TTS -> BT play)
  - Provide system shutdown/restart and health check APIs

  TODOs / Requirements:
  - [ ] Implement a clear state machine for the interaction cycle
  - [ ] Use queues and event groups for IPC; avoid blocking network calls in high-priority tasks
  - [ ] Provide `manager_init()` and `manager_start()` APIs
*/

#include <Arduino.h>

void manager_init();
void manager_start();
void manager_stop();

// For testing: trigger full interaction flow (synchronous test)
void manager_test_interaction();
