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

// Support FP Additional Data extension
#ifndef NEARBY_FP_ENABLE_ADDITIONAL_DATA
#define NEARBY_FP_ENABLE_ADDITIONAL_DATA 1
#endif /* NEARBY_FP_ENABLE_ADDITIONAL_DATA */

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

// Support Smart Audio Source Switching
// #define NEARBY_FP_ENABLE_SASS

// The maximum size in bytes of additional data in a message in Message Stream.
// Bigger payloads will be truncated.
#define MAX_MESSAGE_STREAM_PAYLOAD_SIZE 22

// The maximum number of concurrent RFCOMM connections
#define NEARBY_MAX_RFCOMM_CONNECTIONS 2

// Support Retroactive pairing extension
#ifndef NEARBY_FP_RETROACTIVE_PAIRING
#define NEARBY_FP_RETROACTIVE_PAIRING 1
#endif /* NEARBY_FP_RETROACTIVE_PAIRING */

// The maximum number of concurrent retroactive pairing process
#define NEARBY_MAX_RETROACTIVE_PAIRING 2

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
#endif /* NEARBY_CONFIG_H */
