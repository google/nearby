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

// This module provides a workaround for interleaving Fast Pair and SPOT
// advertisements on platforms that don't have native support.
// The flow when both advertisements are enabled is:
// 1. Start advertising SPOT
// 2. Set a small timer but long enough for one advertisement to be emitted.
// 3. Start advertising Fast Pair
// 4. Set a longer (2 seconds) timer and repeat the cycle.
//
// There is no cycling between advertisements when only one (or none) is
// enabled, of course.
//
// If the platform has native support for mulitiple advertisements, then this
// module is a no-op pass-through to native implementation.
#ifndef NEARBY_ADVERT_H
#define NEARBY_ADVERT_H

// clang-format off
#include "nearby_config.h"
// clang-format on

#include "nearby.h"
#include "nearby_platform_ble.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

nearby_platform_status nearby_SetFpAdvertisement(
    const uint8_t* payload, size_t length,
    nearby_fp_AvertisementInterval interval);

nearby_platform_status nearby_SetSpotAdvertisement(uint64_t address,
    const uint8_t* payload, size_t length);

void nearby_AdvertInit();

#ifdef __cplusplus
}
#endif

#endif /* NEARBY_ADVERT_H */
