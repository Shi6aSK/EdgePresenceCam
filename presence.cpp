#include "presence.h"
#include "stats.h"

static EventGroupHandle_t s_eventGroup = NULL;

void presence_init() {
  if (s_eventGroup == NULL) {
    s_eventGroup = xEventGroupCreate();
    if (s_eventGroup == NULL) {
      Serial.println("presence: failed to create event group");
    } else {
      Serial.println("presence: event group created");
    }
  }
}

void presence_set(bool on) {
  if (s_eventGroup == NULL) return;
  if (on) {
    xEventGroupSetBits(s_eventGroup, PRESENCE_BIT);
  } else {
    xEventGroupClearBits(s_eventGroup, PRESENCE_BIT);
  }
  // update stats
  stats_record_presence_update(on);
}

EventGroupHandle_t presence_get_event_group() {
  return s_eventGroup;
}
