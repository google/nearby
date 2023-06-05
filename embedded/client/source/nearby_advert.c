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

// clang-format off
#include "nearby.h"
#include "nearby_assert.h"
#include "nearby_config.h"
// clang-format on

#include "nearby_advert.h"

#include "nearby_fp_library.h"
#include "nearby_platform_ble.h"
#include "nearby_platform_os.h"
#include "nearby_spot.h"
#include "nearby_trace.h"

#ifdef NEARBY_FP_HAVE_MULTIPLE_ADVERTISEMENTS

nearby_platform_status nearby_SetFpAdvertisement(
    const uint8_t* payload, size_t length,
    nearby_fp_AvertisementInterval interval) {
  return nearby_platform_SetAdvertisement(payload, length, interval);
}

#if NEARBY_FP_ENABLE_SPOT
nearby_platform_status nearby_SetSpotAdvertisement(uint64_t address,
    const uint8_t* payload, size_t length) {
  return nearby_platform_SetSpotAdvertisement(address, payload, length);
}
#endif /* NEARBY_FP_ENABLE_SPOT */

#else
static nearby_platform_status SetSpotAdvertisement();
static nearby_platform_status SetFpAdvertisement();

static uint8_t fp_advertisement[NON_DISCOVERABLE_ADV_SIZE_BYTES];
static size_t fp_length;
static nearby_fp_AvertisementInterval fp_interval;

#if NEARBY_FP_ENABLE_SPOT
// Time needed for the BLE controller to emit one SPOT advertisement. As little
// as 50ms should be enough but it's safer to give the controller a little bit
// more time.
#define SPOT_EMIT_TIME_MS 100
// SPOT advertisements should be emitted every 2 seconds
#define SPOT_EMIT_INTERVAL_MS 2000

typedef enum {
  kRotationNoAdvertisement,
  kRotationFastPair,
  kRotationSpot
} Rotation;
static Rotation rotation = kRotationNoAdvertisement;
static void* timer_task;
static uint8_t spot_advertisement[NEARBY_SPOT_ADVERTISEMENT_SIZE];
static size_t spot_length;
#endif /* NEARBY_FP_ENABLE_SPOT */

static bool HasFpAdvertisement() { return fp_length != 0; }

static bool HasSpotAdvertisement() {
#if NEARBY_FP_ENABLE_SPOT
  return spot_length != 0;
#else
  return false;
#endif /* NEARBY_FP_ENABLE_SPOT */
}

#if NEARBY_FP_ENABLE_SPOT

static void CancelTask() {
  void* task = timer_task;
  timer_task = NULL;
  if (task != NULL) {
    nearby_platform_status status = nearby_platform_CancelTimer(task);
    if (kNearbyStatusOK != status) {
      NEARBY_TRACE(ERROR,
                   "Error canceling advertisement rotation task, status %d",
                   status);
    }
  }
}

static uint32_t GetDelayMs() {
  return rotation == kRotationSpot ? SPOT_EMIT_TIME_MS
                                   : SPOT_EMIT_INTERVAL_MS - SPOT_EMIT_TIME_MS;
}

static void RotateAdvertisements();
static void ScheduleTask() {
  timer_task = nearby_platform_StartTimer(RotateAdvertisements, GetDelayMs());
  if (timer_task == NULL) {
    NEARBY_TRACE(ERROR, "Failed to schedule advertisement rotation");
  }
}

static void RotateAdvertisements() {
  if (rotation == kRotationFastPair) {
    SetSpotAdvertisement();
  } else if (rotation == kRotationSpot) {
    SetFpAdvertisement();
  }
  ScheduleTask();
}
#endif /* NEARBY_FP_ENABLE_SPOT */

static void RescheduleAdvertisementRotation() {
#if NEARBY_FP_ENABLE_SPOT
  CancelTask();
  if (fp_length == 0 || spot_length == 0) {
    // Only one advertisement, no need to rotate
    return;
  }
  ScheduleTask();
#endif /* NEARBY_FP_ENABLE_SPOT */
}

static nearby_platform_status SetSpotAdvertisement() {
#if NEARBY_FP_ENABLE_SPOT
  const void* payload = HasSpotAdvertisement() ? spot_advertisement : NULL;
  rotation = HasSpotAdvertisement() ? kRotationSpot : kRotationNoAdvertisement;
  return nearby_platform_SetAdvertisement(payload, spot_length,
                                          kNoLargerThan2Seconds);
#else
  return kNearbyStatusUnsupported;
#endif /* NEARBY_FP_ENABLE_SPOT */
}

static nearby_platform_status SetFpAdvertisement() {
#if NEARBY_FP_ENABLE_SPOT
  rotation =
      HasFpAdvertisement() ? kRotationFastPair : kRotationNoAdvertisement;
#endif /* NEARBY_FP_ENABLE_SPOT */
  const void* payload = HasFpAdvertisement() ? fp_advertisement : NULL;
  return nearby_platform_SetAdvertisement(payload, fp_length, fp_interval);
}

static nearby_platform_status DisableAdvertisement() {
  return nearby_platform_SetAdvertisement(NULL, 0, kDisabled);
}

nearby_platform_status nearby_SetFpAdvertisement(
    const uint8_t* payload, size_t length,
    nearby_fp_AvertisementInterval interval) {
  nearby_platform_status status;
  NEARBY_ASSERT(length <= sizeof(fp_advertisement));
  if (payload != NULL) memcpy(fp_advertisement, payload, length);
  fp_length = length;
  fp_interval = interval;
  if (HasFpAdvertisement()) {
    status = SetFpAdvertisement();
  } else if (HasSpotAdvertisement()) {
    status = SetSpotAdvertisement();
  } else {
    status = DisableAdvertisement();
  }
  RescheduleAdvertisementRotation();
  return status;
}

#if NEARBY_FP_ENABLE_SPOT
nearby_platform_status nearby_SetSpotAdvertisement(uint64_t address,
    const uint8_t* payload, size_t length) {
  nearby_platform_status status;
  NEARBY_ASSERT(length <= sizeof(spot_advertisement));
  if (payload != NULL) memcpy(spot_advertisement, payload, length);
  spot_length = length;
  if (HasSpotAdvertisement()) {
    status = SetSpotAdvertisement();
  } else if (HasFpAdvertisement()) {
    status = SetFpAdvertisement();
  } else {
    status = DisableAdvertisement();
  }
  RescheduleAdvertisementRotation();
  return status;
}
#endif /* NEARBY_FP_ENABLE_SPOT */

#endif /* NEARBY_FP_HAVE_MULTIPLE_ADVERTISEMENTS */

void nearby_AdvertInit() {
#ifndef NEARBY_FP_HAVE_MULTIPLE_ADVERTISEMENTS
  fp_length = 0;
#if NEARBY_FP_ENABLE_SPOT
  timer_task = NULL;
  spot_length = 0;
#endif /* NEARBY_FP_ENABLE_SPOT */
#endif /* NEARBY_FP_HAVE_MULTIPLE_ADVERTISEMENTS */
}
