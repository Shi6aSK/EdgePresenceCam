#pragma once

/*
  TTS module skeleton

  Responsibilities:
  - Convert text to audio (PCM/WAV) using cloud TTS API (OpenAI if available or Google TTS)
  - Provide an API to produce PCM buffers or save WAV files
  - Optionally perform local TTS if a lightweight model is used (rare on ESP32-S3)

  TODOs / Requirements:
  - [ ] Implement `tts_synthesize_text(prompt, callback)` to request TTS from cloud
  - [ ] Support returning PCM buffer or saving to file for A2DP streaming
  - [ ] Provide options for sample rate and channels compatible with A2DP / I2S
*/

#include <Arduino.h>
using tts_callback_t = void(*)(bool ok, const char* wav_path);

void tts_init();
void tts_synthesize_text(const char* text, tts_callback_t cb);
