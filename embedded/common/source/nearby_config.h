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
#define NEARBY_FP_ENABLE_BATTERY_NOTIFICATION

// Support FP Additional Data extension
#define NEARBY_FP_ENABLE_ADDITIONAL_DATA

// Personalized name max size in bytes
#define PERSONALIZED_NAME_MAX_SIZE 64

// Support FP Message Stream extension
#define NEARBY_FP_MESSAGE_STREAM

// Does the platform have a native BLE address rotation routine?
// #define NEARBY_FP_HAVE_BLE_ADDRESS_ROTATION

// The maximum size in bytes of additional data in a message in Message Stream.
// Bigger payloads will be truncated.
#define MAX_MESSAGE_STREAM_PAYLOAD_SIZE 8

// The maximum number of concurrent RFCOMM connections
#define NEARBY_MAX_RFCOMM_CONNECTIONS 2

// Support Retroactive pairing extension
#define NEARBY_FP_RETROACTIVE_PAIRING

// The maximum number of concurrent retroactive pairing process
#define NEARBY_MAX_RETROACTIVE_PAIRING 2

#endif /* NEARBY_CONFIG_H */
