// Simple simulated/capture audio implementation with WAV write to LittleFS.
// This implementation uses a memory buffer to store a short capture and
// demonstrates the end-to-end flow: button -> capture -> save -> HTTP download.

#include "audio.h"
#include <LittleFS.h>
#include <math.h>
#include "log.h"

static bool s_inited = false;
static volatile bool s_recording = false;
static uint32_t s_sample_rate = 16000;
static uint8_t *s_buffer = NULL;
static size_t s_buffer_len = 0;
static const char *s_last_path = "/audio/latest.wav";

static void write_wav_header(File &f, uint32_t sample_rate, uint16_t bits_per_sample, uint16_t channels, uint32_t data_len) {
  uint32_t byte_rate = sample_rate * channels * (bits_per_sample / 8);
  uint16_t block_align = channels * (bits_per_sample / 8);
  // RIFF header
  f.write((const uint8_t*)"RIFF", 4);
  uint32_t chunk_size = 36 + data_len;
  f.write((const uint8_t *)&chunk_size, 4);
  f.write((const uint8_t*)"WAVE", 4);
  // fmt subchunk
  f.write((const uint8_t*)"fmt ", 4);
  uint32_t subchunk1 = 16;
  f.write((const uint8_t *)&subchunk1, 4);
  uint16_t audio_format = 1; // PCM
  f.write((const uint8_t *)&audio_format, 2);
  f.write((const uint8_t *)&channels, 2);
  f.write((const uint8_t *)&sample_rate, 4);
  f.write((const uint8_t *)&byte_rate, 4);
  f.write((const uint8_t *)&block_align, 2);
  f.write((const uint8_t *)&bits_per_sample, 2);
  // data subchunk
  f.write((const uint8_t*)"data", 4);
  f.write((const uint8_t *)&data_len, 4);
}

void audio_set_sample_rate(uint32_t sr) {
  s_sample_rate = sr;
}

void audio_init() {
  if (s_inited) return;
  if (!LittleFS.begin()) {
    Serial.println("audio: LittleFS mount failed, attempting format+mount");
    log_append("audio: LittleFS mount failed, attempting format");
    // Try mount with format fallback (some LittleFS implementations accept a 'formatOnFail' flag)
    if (!LittleFS.begin(true)) {
      Serial.println("audio: LittleFS format+mount failed");
      log_append("audio: LittleFS format+mount failed");
    } else {
      Serial.println("audio: LittleFS mounted after format");
      log_append("audio: LittleFS mounted after format");
    }
  } else {
    Serial.println("audio: LittleFS mounted");
    log_append("audio: LittleFS mounted");
  }
  s_inited = true;
}

// Simulate capture by generating a simple sine wave PCM16 for the requested duration
void audio_start_recording(uint32_t duration_ms) {
  if (!s_inited) audio_init();
  if (s_recording) return;
  size_t samples = (size_t)((uint64_t)s_sample_rate * duration_ms / 1000ULL);
  size_t bytes = samples * 2; // 16-bit mono
  // clamp to reasonable size (e.g., 512KB)
  if (bytes > 512 * 1024) bytes = 512 * 1024;
  if (s_buffer) free(s_buffer);
  s_buffer = (uint8_t*)malloc(bytes);
  if (!s_buffer) {
    Serial.println("audio: failed to allocate buffer");
    return;
  }
  s_buffer_len = bytes;
  // generate sine
  const double freq = 440.0;
  for (size_t i = 0; i < samples; ++i) {
    double t = (double)i / (double)s_sample_rate;
    double v = sin(2.0 * M_PI * freq * t) * 0.25; // small amplitude
    int16_t s = (int16_t)(v * 32767.0);
    s_buffer[2*i] = s & 0xFF;
    s_buffer[2*i+1] = (s >> 8) & 0xFF;
  }
  s_recording = true;
  log_append("audio: simulated recording started dur=%lu ms size=%uB", (unsigned long)duration_ms, (unsigned)bytes);

  // Save WAV synchronously (for demo) — in real system, spawn a task to write
  File f = LittleFS.open(s_last_path, FILE_WRITE);
  if (!f) {
    log_append("audio: failed to open %s for writing", s_last_path);
    Serial.println("audio: failed to open file");
    s_recording = false;
    return;
  }
  write_wav_header(f, s_sample_rate, 16, 1, (uint32_t)s_buffer_len);
  f.write(s_buffer, s_buffer_len);
  f.close();
  s_recording = false;
  log_append("audio: saved %s (%u bytes)", s_last_path, (unsigned)s_buffer_len);
}

void audio_stop_recording() {
  if (!s_recording) return;
  s_recording = false;
}

bool audio_is_recording() {
  return s_recording;
}

bool audio_save_to_file(const char* path) {
  if (!s_buffer || s_buffer_len == 0) return false;
  if (!LittleFS.begin()) LittleFS.begin();
  File f = LittleFS.open(path, FILE_WRITE);
  if (!f) return false;
  write_wav_header(f, s_sample_rate, 16, 1, (uint32_t)s_buffer_len);
  f.write(s_buffer, s_buffer_len);
  f.close();
  return true;
}

void audio_test_capture_and_save() {
  audio_start_recording(3000);
}
