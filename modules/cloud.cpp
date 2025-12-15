#include "cloud.h"
#include "config.h"

/*
  TODOs / Implementation notes:
  - Use `HTTPClient` or `WiFiClientSecure` for HTTPS requests on Arduino-ESP32.
  - Ensure `config.h` provides a secure way to load the OpenAI API key (e.g., from SPIFFS/secret file).
  - For large audio files, stream the file in chunks (avoid loading entire file into RAM).
  - Implement callbacks to notify manager when transcription and GPT responses are ready.
*/

static bool s_inited = false;

void cloud_init() {
  if (s_inited) return;
  // TODO: configure TLS root CA if necessary
  s_inited = true;
}

void cloud_send_audio(const char* wav_path, cloud_callback_t cb) {
  // TODO: open file, send to Whisper/ASR endpoint, parse result, call cb
  (void)wav_path; (void)cb;
}

void cloud_send_text(const char* prompt, cloud_callback_t cb) {
  // TODO: POST prompt to ChatGPT (OpenAI) and return response via cb
  (void)prompt; (void)cb;
}

String cloud_send_text_blocking(const char* prompt, uint32_t timeout_ms) {
  // TODO: implement blocking helper for quick testing. Use cloud_send_text + synchronization.
  (void)prompt; (void)timeout_ms;
  return String("(not implemented)");
}
