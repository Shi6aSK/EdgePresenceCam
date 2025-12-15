#pragma once

/*
  Cloud module skeleton (OpenAI APIs)

  Responsibilities:
  - Upload recorded audio (WAV/PCM) to OpenAI Whisper endpoint (ASR)
  - Send transcribed text to ChatGPT (NLG) with a system prompt
  - Provide an HTTP client helper with TLS verification and token management
  - Expose async APIs so cloud tasks run in their own FreeRTOS task

  Requirements / TODOs:
  - [ ] Securely store API key (config.h, or read from file). Never hardcode secrets in repo.
  - [ ] Implement HTTPS POST with multipart/form-data for audio upload.
  - [ ] Implement JSON parsing for responses (ArduinoJson recommended) — but keep payloads small.
  - [ ] Provide `cloud_init()`, `cloud_send_audio(file_path, callback)`, `cloud_send_text(prompt, callback)` APIs.
  - [ ] Make timeouts and retry policies configurable.
*/

#include <Arduino.h>

using cloud_callback_t = void(*)(bool ok, const String &response);

void cloud_init();
void cloud_send_audio(const char* wav_path, cloud_callback_t cb);
void cloud_send_text(const char* prompt, cloud_callback_t cb);

// Helper: block until response (blocking alternative for tests)
String cloud_send_text_blocking(const char* prompt, uint32_t timeout_ms = 10000);
