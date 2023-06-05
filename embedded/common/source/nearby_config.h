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

#ifndef NEARBY_CONFIG_H
#define NEARBY_CONFIG_H

// Support FP Battery Notification extension
#ifndef NEARBY_FP_ENABLE_BATTERY_NOTIFICATION
#define NEARBY_FP_ENABLE_BATTERY_NOTIFICATION 1
#endif /* NEARBY_FP_ENABLE_BATTERY_NOTIFICATION */

// Define number of battery component used
#define NEARBY_BATTERY_LEVELS_SIZE 3

// Support FP Battery Remaining Time
#ifndef NEARBY_BATTERY_REMAINING_TIME
#define NEARBY_BATTERY_REMAINING_TIME 1
#endif /* NEARBY_BATTERY_REMAINING_TIME */

// Support FP Additional Data extension
#ifndef NEARBY_FP_ENABLE_ADDITIONAL_DATA
#define NEARBY_FP_ENABLE_ADDITIONAL_DATA 1
#endif /* NEARBY_FP_ENABLE_ADDITIONAL_DATA */

// Support Eddystone-E2EE-EID aka SPOT
#ifndef NEARBY_FP_ENABLE_SPOT
#define NEARBY_FP_ENABLE_SPOT 1
#endif /* NEARBY_FP_ENABLE_SPOT */

// Personalized name max size in bytes
#define PERSONALIZED_NAME_MAX_SIZE 64

// Support FP Message Stream extension
#ifndef NEARBY_FP_MESSAGE_STREAM
#define NEARBY_FP_MESSAGE_STREAM 1
#endif /* NEARBY_FP_MESSAGE_STREAM */

// Number of salt bytes to use in advertisements
#define NEARBY_FP_SALT_SIZE 2

// Does the platform have a native BLE address rotation routine?
// #define NEARBY_FP_HAVE_BLE_ADDRESS_ROTATION

// Does the platform support interleaving LE advertisements?
// If yes, the controller can be configured to send both FP and SPOT
// advertisements at different intervals. The platform should implemnt both
// `nearby_platform_SetAdvertisement()` and
// `nearby_platform_SetSpotAdvertisement()` in this case.
//
// If not, Nearby SDK will alternate between FP and SPOT advertisements. The
// plaftorm should implement only `nearby_platform_SetAdvertisement()` in this
// case.
#define NEARBY_FP_HAVE_MULTIPLE_ADVERTISEMENTS

// Support Smart Audio Source Switching
#ifndef NEARBY_FP_ENABLE_SASS
#define NEARBY_FP_ENABLE_SASS 1
#endif /* NEARBY_FP_ENABLE_SASS */

// The maximum size in bytes of additional data in a message in Message Stream.
// Bigger payloads will be truncated.
#define MAX_MESSAGE_STREAM_PAYLOAD_SIZE 36

// The maximum number of concurrent RFCOMM connections
#define NEARBY_MAX_RFCOMM_CONNECTIONS 2

// Support Retroactive pairing extension
#ifndef NEARBY_FP_RETROACTIVE_PAIRING
#define NEARBY_FP_RETROACTIVE_PAIRING 1
#endif /* NEARBY_FP_RETROACTIVE_PAIRING */

// The maximum number of concurrent retroactive pairing process
#define NEARBY_MAX_RETROACTIVE_PAIRING 2

#if NEARBY_FP_ENABLE_SPOT
// Add Battery level indication in Spot Advertisement
#define NEARBY_SPOT_BATTERY_LEVEL_INDICATION

// Battery level threshold macros to define Normal, low and critically low
#define NEARBY_SPOT_BATTERY_LEVEL_LOW_THRESHOLD 20
#define NEARBY_SPOT_BATTERY_LEVEL_CRITICALLY_LOW_THRESHOLD 10

// Define size of the curve used by SPOT: 20 (BT4.2) for SECP160R1  and 32
// (BT 5.0 & above) for SECP256R1
#define NEARBY_SPOT_SECP_CURVE_SIZE 20

// Reset the device to factory defaults when Ephemeral Identity Key is
// cleared for Locator Tags
#ifndef NEARBY_SPOT_FACTORY_RESET_DEVICE_ON_CLEARING_EIK
#define NEARBY_SPOT_FACTORY_RESET_DEVICE_ON_CLEARING_EIK 0
#endif

#endif /* NEARBY_FP_ENABLE_SPOT */

// Is this platform a BLE-only device?
#ifndef NEARBY_FP_BLE_ONLY
#define NEARBY_FP_BLE_ONLY 0
#endif /* NEARBY_FP_BLE_ONLY */

// Does this device prefer BLE bonding?
#ifndef NEARBY_FP_PREFER_BLE_BONDING
#define NEARBY_FP_PREFER_BLE_BONDING 0
#endif /* NEARBY_FP_PREFER_BLE_BONDING */

// Does this device prefer LE transport for FP Message Stream?
// When this feature is On, the Seeker will try to connect to the provider over
// an L2CAP channel. When this feature is Off, the Seeker connects over RFCOMM.
#ifndef NEARBY_FP_PREFER_LE_TRANSPORT
#define NEARBY_FP_PREFER_LE_TRANSPORT 0
#endif /* NEARBY_FP_PREFER_LE_TRANSPORT */

// The maximum number of account keys that can be stored on the device.
#define NEARBY_MAX_ACCOUNT_KEYS 5

// When this flag is On, if the Seeker disconnects from the rfcomm/l2cap
// endpoint during pairing, the pairing state will be reset, and pairing attempt
// terminated.
#ifndef NEARBY_FP_RESET_PAIRING_STATE_ON_DISCONNECT
#define NEARBY_FP_RESET_PAIRING_STATE_ON_DISCONNECT 1
#endif /* NEARBY_FP_RESET_PAIRING_STATE_ON_DISCONNECT */

// Set to 1 if the platform supports sending advertisements from different
// BLE addresses at the same time.
#ifndef NEARBY_SUPPORT_MULTIPLE_BLE_ADDRESSES
#define NEARBY_SUPPORT_MULTIPLE_BLE_ADDRESSES 0
#endif /* NEARBY_SUPPORT_MULTIPLE_BLE_ADDRESSES */

#endif /* NEARBY_CONFIG_H */
