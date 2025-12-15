#pragma once

/*
  Audio module (I2S) skeleton

  Responsibilities:
  - Configure I2S peripheral for the board's digital microphone
  - Provide a ring buffer / DMA-friendly buffer API for recorded audio chunks
  - Support start/stop recording operations triggered by the manager
  - Write recorded audio to SPIFFS/LittleFS (WAV header) for later upload or streaming
  - Offer a small test routine to capture N seconds and save to file

  Requirements / TODOs:
  - [ ] Implement I2S init with correct GPIO pins for XIAO ESP32-S3
  - [ ] Implement a lock-free (or mutex-protected) circular buffer for audio chunks
  - [ ] Implement WAV header writer and file writer to LittleFS
  - [ ] Provide configurable sample rate / bit depth constants
  - [ ] Expose `audio_init()`, `audio_start_recording(duration_ms)`, `audio_stop_recording()`, `audio_save_to_file(path)` APIs
  - [ ] Provide callbacks or queue-based notification when audio chunk ready (for cloud upload)
*/

#include <Arduino.h>

void audio_init();
void audio_start_recording(uint32_t duration_ms);
void audio_stop_recording();
bool audio_is_recording();

// Save last captured buffer to a file (WAV). Returns true on success.
bool audio_save_to_file(const char* path);

// Test helper: capture short clip and save
void audio_test_capture_and_save();

// Configure simulated/sample rate for generated test audio (used when I2S not configured)
void audio_set_sample_rate(uint32_t sr);
