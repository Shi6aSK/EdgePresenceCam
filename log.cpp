#include "log.h"

static const int LOG_ENTRIES = 64;
static const int LOG_ENTRY_LEN = 128;
static char s_logbuf[LOG_ENTRIES][LOG_ENTRY_LEN];
static int s_log_idx = 0;
static portMUX_TYPE s_log_lock = portMUX_INITIALIZER_UNLOCKED;

void log_init() {
  portENTER_CRITICAL(&s_log_lock);
  for (int i = 0; i < LOG_ENTRIES; ++i) s_logbuf[i][0] = '\0';
  s_log_idx = 0;
  portEXIT_CRITICAL(&s_log_lock);
}

void log_append(const char* fmt, ...) {
  char tmp[LOG_ENTRY_LEN];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(tmp, sizeof(tmp), fmt, ap);
  va_end(ap);

  portENTER_CRITICAL(&s_log_lock);
  strncpy(s_logbuf[s_log_idx], tmp, LOG_ENTRY_LEN-1);
  s_logbuf[s_log_idx][LOG_ENTRY_LEN-1] = '\0';
  s_log_idx = (s_log_idx + 1) % LOG_ENTRIES;
  portEXIT_CRITICAL(&s_log_lock);
}

String log_get_json() {
  String out = "[";
  portENTER_CRITICAL(&s_log_lock);
  int idx = s_log_idx;
  for (int i = 0; i < LOG_ENTRIES; ++i) {
    int j = (idx + i) % LOG_ENTRIES;
    if (s_logbuf[j][0] == '\0') continue;
    if (out.length() > 1) out += ",";
    // escape double quotes in entry
    String e = String(s_logbuf[j]);
    e.replace("\\", "\\\\");
    e.replace("\"", "\\\"");
    out += "\"" + e + "\"";
  }
  portEXIT_CRITICAL(&s_log_lock);
  out += "]";
  return out;
}
