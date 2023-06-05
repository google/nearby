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

#ifndef NEARBY_SPOT_H
#define NEARBY_SPOT_H

// clang-format off
#include "nearby_config.h"
// clang-format on

#include "nearby.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#if NEARBY_FP_ENABLE_SPOT

#if (NEARBY_SPOT_SECP_CURVE_SIZE == 20)
#define NEARBY_SPOT_ADVERTISEMENT_SIZE 29
#define EPHEMERAL_ID_SIZE 20
#else
#define NEARBY_SPOT_ADVERTISEMENT_SIZE 41
#define EPHEMERAL_ID_SIZE 32
#endif /* NEARBY_SPOT_SECP_CURVE_SIZE */

// Define protocol version for SPOT protocol
#define SPOT_BEACON_PROTOCOL_MAJOR_VERSION 1

#ifdef NEARBY_SPOT_BATTERY_LEVEL_INDICATION
#define MAIN_BATTERY_LEVEL_INDEX 0
#endif /* NEARBY_SPOT_BATTERY_LEVEL_INDICATION */

// The BLE address should be rotated on average every 24 hours (86400 seconds)
#define ADDRESS_ROTATION_PERIOD_UNWANTED_TRACKING_PROTECTION_MS 86400000U

// Initializes SPOT library. Reads configuration from Flash, doesn't start
// advertising, though.
nearby_platform_status nearby_spot_Init();

// Sets the "owner" account key. The owner account key is the first account key
// added to the device
nearby_platform_status nearby_spot_SetBeaconAccountKey(
    const uint8_t key[ACCOUNT_KEY_SIZE_BYTES]);

// Creates SPOT advertisement and starts advertising when `enable` is true. When
// `enable` is false, stops advertising. nearby_spot_SetAdvertisement should be
// called, on average, every 1024 seconds. It should also be called when BLE
// address changes.
nearby_platform_status nearby_spot_SetAdvertisement(bool enable);

// Activates or Deactivates the Unwanted TrackingProtection Mode
// Pass true to activate or false to deactivate
void nearby_spot_SetUnwantedTrackingProtectionMode(bool activate);

// Gets the current status of Unwanted Tracking Mode
// output is true if activated or false if unactivated
bool nearby_spot_GetUnwantedTrackingProtectionModeState(void);

// Returns the current value of control flags:
// 0x01 - Skip Ringing Authentication
// 0x00 - No flag set
uint8_t nearby_spot_GetControlFlags(void);

// A callback for reading from Beacon Action characteristic.
nearby_platform_status nearby_spot_ReadBeaconAction(uint64_t peer_address,
                                                    uint8_t* output,
                                                    size_t* length);

// A callback for writing to Beacon Action characteristic.
nearby_platform_status nearby_spot_WriteBeaconAction(uint64_t peer_address,
                                                     const uint8_t* request,
                                                     size_t length);

// A callback that should be triggered when the device starts or stops ringing.
nearby_platform_status nearby_spot_OnRingStateChange();

// Returns true if the device is provisionned by any account.
bool nearby_spot_IsProvisioned();

// Copies the Ephemeral ID into the input eid_ptr
nearby_platform_status nearby_spot_GetEid(uint8_t* eid_ptr);

#if NEARBY_SUPPORT_MULTIPLE_BLE_ADDRESSES
// Generates the SPOT randomized address
void nearby_spot_GenerateRandomizedAddress();

// Returns the timestamp when the address was rotated
unsigned int nearby_spot_GetAddressRotationTimestamp();
#endif /* NEARBY_SUPPORT_MULTIPLE_BLE_ADDRESSES */

#endif /* NEARBY_FP_ENABLE_SPOT */
#ifdef __cplusplus
}
#endif

#endif /* NEARBY_SPOT_H */
