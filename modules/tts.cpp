#include "tts.h"
#include "cloud.h"

/*
  TODOs / Implementation notes:
  - tts_synthesize_text should call `cloud_send_text` or a dedicated TTS endpoint
  - Store returned audio as a WAV file and call callback with file path
  - Ensure proper format and sample rate for A2DP streaming
*/

static bool s_inited = false;

void tts_init() {
  if (s_inited) return;
  s_inited = true;
}

void tts_synthesize_text(const char* text, tts_callback_t cb) {
  // For now, stub: call cloud_send_text then produce dummy wav path via cb
  cloud_send_text(text, [cb](bool ok, const String &response){
    // TODO: convert `response` text to audio via TTS API and write to file
    if (cb) cb(false, "");
  });
}
