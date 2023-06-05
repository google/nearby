// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <algorithm>
#include <cstddef>
#include <limits>
#include <map>

#include "nearby.h"
#include "nearby_assert.h"
#include "nearby_fp_library.h"
#include "nearby_platform_os.h"

const char firmware_revision[] = {'1', '.', '0', '.', '0', '\0'};
bool has_user_consent_for_reading_eik = false;

typedef void (*timer)();
struct TimerInfo {
  TimerInfo(timer callback, uint32_t trigger_time)
      : callback_(callback), trigger_time_(trigger_time) {}
  timer callback_;
  uint32_t trigger_time_;
  bool has_run_ = false;
};
static std::map<long, TimerInfo> timers;
static long timer_key = 0;
static uint32_t current_time;
static const nearby_platform_OsInterface* callbacks;
static uint32_t seconds = 50000;
static constexpr nearby_platform_RingingInfo kDefaultRingingInfo = {
    .ring_state = kRingStateStoppedReasonTimeout,
    .num_components = 3,
    .components = 0,
    .timeout = 0,
#if NEARBY_FP_ENABLE_SPOT
    .volume = kRingingVolumeDefault,
#endif /* NEARBY_FP_ENABLE_SPOT */
};

static nearby_platform_RingingInfo ringing_info = kDefaultRingingInfo;

// Gets current time in ms.
unsigned int nearby_platform_GetCurrentTimeMs() { return current_time; }

uint32_t nearby_platform_GetPersistentTime() { return seconds; }

nearby_platform_status nearby_platform_GetRingingInfo(
    nearby_platform_RingingInfo* ringing_info) {
  *ringing_info = ::ringing_info;
  return kNearbyStatusOK;
}

nearby_platform_status nearby_platform_Ring(
    uint8_t command, uint16_t timeout, nearby_platform_RingingVolume volume) {
  ringing_info.components = command;
  ringing_info.timeout = timeout;
#if NEARBY_FP_ENABLE_SPOT
  ringing_info.volume = volume;
#endif /* NEARBY_FP_ENABLE_SPOT */
  ringing_info.ring_state = kRingStateStarted;
  return kNearbyStatusOK;
}

#if NEARBY_FP_ENABLE_SPOT
/* returns user consent as true if reading of EIK is permitted (when in 
EIK recovery mode) by a button press or other user action */
bool nearby_platform_HasUserConsentForReadingEik(void)
{
  return has_user_consent_for_reading_eik;
}

void nearby_test_fakes_SetHasUserContentForReadingEik(bool has_user_consent) {
  has_user_consent_for_reading_eik = has_user_consent;
}

/* Perform a factory reset and clear all stored Account Keys */
nearby_platform_status nearby_platform_FactoryReset() {
  // Clear all the account keys
  return nearby_fp_ClearAccountKeys();
}
#endif /* NEARBY_FP_ENABLE_SPOT */

// Starts a timer. Returns an opaque timer handle or null on error.
void* nearby_platform_StartTimer(void (*callback)(), unsigned int delay_ms) {
  long key = timer_key++;
  timers.emplace(std::make_pair(
      key, TimerInfo(callback, nearby_platform_GetCurrentTimeMs() + delay_ms)));
  return reinterpret_cast<void*>(key);
}

// Cancels a timer
nearby_platform_status nearby_platform_CancelTimer(void* timer) {
  long key = reinterpret_cast<long>(timer);
  auto timer_info = timers.find(key);
  NEARBY_ASSERT(timer_info != timers.end());
  timers.erase(timer_info);
  return kNearbyStatusOK;
}

uint32_t nearby_test_fakes_GetNextTimerMs() {
  uint32_t timer_trigger_time = std::numeric_limits<uint32_t>::max();
  std::for_each(
      timers.begin(), timers.end(),
      [&timer_trigger_time](std::pair<long, TimerInfo> elem) {
        auto timer = elem.second;
        if (!timer.has_run_ && timer.trigger_time_ < timer_trigger_time) {
          timer_trigger_time = timer.trigger_time_;
        }
      });
  // Returns 0 if there are no timers set
  return timer_trigger_time != std::numeric_limits<uint32_t>::max()
             ? timer_trigger_time
             : 0;
}

void nearby_test_fakes_SetCurrentTimeMs(uint32_t ms) {
  current_time = ms;
  std::for_each(timers.begin(), timers.end(),
                [ms](std::pair<long, TimerInfo> elem) {
                  auto timer = elem.second;
                  if (!timer.has_run_ && timer.callback_ != NULL &&
                      timer.trigger_time_ <= ms) {
                    timer.has_run_ = true;
                    timer.callback_();
                  }
                });
}

void nearby_test_fakes_SetRingingInfo(const nearby_platform_RingingInfo* info) {
  ringing_info = *info;
}

uint8_t nearby_test_fakes_GetRingCommand(void) {
  return ringing_info.components;
}

uint16_t nearby_test_fakes_GetRingTimeout(void) { return ringing_info.timeout; }

void nearby_test_fakes_NotifyRingStateChanged() {
  callbacks->on_ring_state_change();
}

nearby_platform_status nearby_platform_OsInit(
    const nearby_platform_OsInterface* os_interface) {
  callbacks = os_interface;
  current_time = 0;
  timers.clear();
  ringing_info = kDefaultRingingInfo;
  has_user_consent_for_reading_eik = false;
  return kNearbyStatusOK;
}

// Gets the firmware string
const char* nearby_platform_GetFirmwareRevision(void) {
  return firmware_revision;
}