#include "manager.h"
#include "audio.h"
#include "cloud.h"
#include "tts.h"
#include "bluetooth.h"
#include "can.h"
#include "presence.h"
#include "camera.h"
#include "stats.h"

/*
  High-level manager skeleton

  TODOs:
  - implement event queue and state machine
  - ensure cloud calls are done in network worker task
  - ensure A2DP streaming is driven by a dedicated BT task
*/

static bool s_inited = false;

void manager_init() {
  if (s_inited) return;
  // initialize modules (no-op safe)
  audio_init();
  cloud_init();
  tts_init();
  bt_init();
  can_init();
  s_inited = true;
}

void manager_start() {
  // TODO: create worker tasks, queues, and event subscriptions
}

void manager_stop() {
  // TODO: gracefully stop workers
}

void manager_test_interaction() {
  // TODO: orchestration test: simulate presence -> capture -> ASR -> GPT -> TTS -> BT play
}
