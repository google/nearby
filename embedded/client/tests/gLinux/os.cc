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

#include <cstddef>

#include "nearby_assert.h"
#include "nearby_platform_os.h"

typedef void (*timer)();
static timer timer_callback;
static uint32_t timer_trigger_time;
static bool timer_has_run;
static uint32_t current_time;
static constexpr nearby_platform_RingingInfo kDefaultRingingInfo = {
    .ring_state = kRingStateStoppedReasonTimeout,
    .num_components = 3,
    .components = 0,
    .timeout = 0,
};

static nearby_platform_RingingInfo ringing_info = kDefaultRingingInfo;

// Gets current time in ms.
unsigned int nearby_platform_GetCurrentTimeMs() { return current_time; }

// Starts a timer. Returns an opaque timer handle or null on error.
void* nearby_platform_StartTimer(void (*callback)(), unsigned int delay_ms) {
  timer_callback = callback;
  timer_trigger_time = nearby_platform_GetCurrentTimeMs() + delay_ms;
  timer_has_run = false;
  return (void*)timer_callback;
}

// Cancels a timer
nearby_platform_status nearby_platform_CancelTimer(void* timer) {
  NEARBY_ASSERT(timer == timer_callback);
  timer_callback = NULL;
  timer_trigger_time = 0;
  return kNearbyStatusOK;
}

uint32_t nearby_test_fakes_GetNextTimerMs() { return timer_trigger_time; }

void nearby_test_fakes_SetCurrentTimeMs(uint32_t ms) {
  current_time = ms;
  if (!timer_has_run && timer_callback != NULL && timer_trigger_time <= ms) {
    timer_has_run = true;
    timer_callback();
  }
}

nearby_platform_status nearby_platform_Ring(uint8_t command, uint16_t timeout) {
  ringing_info.components = command;
  ringing_info.timeout = timeout;
  ringing_info.ring_state = kRingStateStarted;
  return kNearbyStatusOK;
}

uint8_t nearby_test_fakes_GetRingCommand(void) {
  return ringing_info.components;
}

uint16_t nearby_test_fakes_GetRingTimeout(void) { return ringing_info.timeout; }

nearby_platform_status nearby_platform_OsInit() {
  current_time = 0;
  timer_callback = NULL;
  timer_trigger_time = 0;
  timer_has_run = false;
  return kNearbyStatusOK;
}
