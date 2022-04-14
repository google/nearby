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

#ifndef NEARBY_PLATFORM_OS_H
#define NEARBY_PLATFORM_OS_H

#include "nearby.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef enum {
  kRingStateStarted = 0,
  kRingStateFailedToStartOrStop,
  kRingStateStoppedReasonTimeout,
  kRingStateStoppedReasonUserAction,
  kRingStateStoppedReasonNearbyRequest
} nearby_platform_RingState;

typedef struct {
  nearby_platform_RingState ring_state;
  // The number of components capable of ringing
  // 0 - the device is incapable of ringing
  // 1 - only a single component is capable of ringing
  // 2 - left and right bud can ring
  // 3 - left and right bud, and the case can ring
  uint8_t num_components;
  // A bitmap indicating what (sub)components are ringing. See
  // `nearby_platform_Ring` for details
  uint8_t components;
  // Remaining ringing timeout in deciseconds
  uint16_t timeout;
} nearby_platform_RingingInfo;

// Gets current time in ms.
unsigned int nearby_platform_GetCurrentTimeMs();

// Starts a timer. Returns an opaque timer handle or null on error.
//
// callback - Function to call when timer matures.
// delay_ms - Number of milliseconds to run the timer.
void* nearby_platform_StartTimer(void (*callback)(), unsigned int delay_ms);

// Cancels a timer
//
// timer - Timer handle returned by StartTimer.
nearby_platform_status nearby_platform_CancelTimer(void* timer);

// Initializes OS module
nearby_platform_status nearby_platform_OsInit();

// Starts ringing
//
// `command` - the requested ringing state as a bitmap:
// Bit 1 (0x01): ring right
// Bit 2 (0x02): ring left
// Bit 3 (0x04): ring case
// Alternatively, `command` hold a special value of 0xFF to ring all
// components that can ring.
// `timeout` - the timeout in deciseconds. The timeout overrides the one already
// in effect if any component of the device is already ringing.
nearby_platform_status nearby_platform_Ring(uint8_t command, uint16_t timeout);

#ifdef __cplusplus
}
#endif

#endif /* NEARBY_PLATFORM_OS_H */
