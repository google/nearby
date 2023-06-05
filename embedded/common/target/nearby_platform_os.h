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

// clang-format off
#include "nearby_config.h"
// clang-format on
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

typedef enum {
  kRingingVolumeDefault = 0,
  kRingingVolumeLow,
  kRingingVolumeMedium,
  kRingingVolumeHigh
} nearby_platform_RingingVolume;

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
  // A byte indicates the ringing volume :
  // 0x00 : default
  // 0x01 : low
  // 0x02 : medium
  // 0x03 : high
#if NEARBY_FP_ENABLE_SPOT
  nearby_platform_RingingVolume volume;
#endif
} nearby_platform_RingingInfo;

typedef struct {
  // Notifies Nearby SDK that ring state has changed
  void (*on_ring_state_change)();
} nearby_platform_OsInterface;

// Gets current time in ms.
unsigned int nearby_platform_GetCurrentTimeMs();

#if NEARBY_FP_ENABLE_SPOT
// Gets current, persistent time in seconds.
// This clock must be (at least) 32 bit. Since the resolving of the Eddystone
// identifier is strongly tied to its clock value at the time of the
// advertisement, it is important that the Provider is able to recover its clock
// value in the event of power loss. We recommend that it writes its current
// clock value to non-volatile memory once per day, and that at boot time it
// checks the NVM to see if there's a value present from which to initialise.
// Resolvers of the ephemeral ID would implement resolution over a time window
// sufficient to allow for both reasonable clock drift and this type of power
// loss recovery.
uint32_t nearby_platform_GetPersistentTime();

nearby_platform_status nearby_platform_GetRingingInfo(
    nearby_platform_RingingInfo* ringing_info);

// Returns true if the device has consented to read EIK (Ephemeral Identity Key)
// typically done during EIK recovery mode. The EIK recovery mode can be entered for
// some limited time after a physical button was pressed on the device (which constitutes the “user
// consent”).
bool nearby_platform_HasUserConsentForReadingEik(void);

// Perform factory reset of device
nearby_platform_status nearby_platform_FactoryReset(void);
#endif /* NEARBY_FP_ENABLE_SPOT */

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
// 'volume' - the requested ringing volume :
// 0x00 : default
// 0x01 : low
// 0x02 : medium
// 0x03 : high
nearby_platform_status nearby_platform_Ring(
    uint8_t command, uint16_t timeout, nearby_platform_RingingVolume volume);

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
nearby_platform_status nearby_platform_OsInit(
    const nearby_platform_OsInterface* os_interface);

// Returns the firmware string
const char* nearby_platform_GetFirmwareRevision(void);
#ifdef __cplusplus
}
#endif

#endif /* NEARBY_PLATFORM_OS_H */
