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

#ifndef NEARBY_EVENT_H
#define NEARBY_EVENT_H

#include "nearby.h"

// Types of events that can be emitted by Nearby library
typedef enum {
  // A client connected over RFCOMM to the message stream endpoint defined in
  // Fast Pair specification
  // Payload: |nearby_event_MessageStreamConnected|
  kNearbyEventMessageStreamConnected = 0,
  // An RFCOMM client disconnected
  // Payload: |nearby_event_MessageStreamDisonnected|
  kNearbyEventMessageStreamDisconnected,
  // A client has sent a message over the message stream. Some incoming messages
  // are consumed by the Nearby library, in which case the event is not emitted.
  // Payload: |nearby_event_MessageStreamReceived|
  kNearbyEventMessageStreamReceived
} nearby_event_Type;

// An event emitted by Nearby library
typedef struct {
  // The type of the event
  nearby_event_Type event_type;
  // Optional event payload. |payload| should be cast to one the structs defined
  // below.
  uint8_t *payload;
} nearby_event_Event;

// Payload for |kNearbyEventMessageStreamConnected| event.
typedef struct {
  // The client BT address
  uint64_t peer_address;
} nearby_event_MessageStreamConnected;

// Payload for |kNearbyEventMessageStreamDisconnected| event.
typedef struct {
  // The client BT address
  uint64_t peer_address;
} nearby_event_MessageStreamDisconnected;

// Message group Bluetooth
#define MESSAGE_GROUP_BLUETOOTH 1

// If the device supports silence mode, the client app should send either
// ENABLE_SILENCE_MODE or DISABLE_SILENCE_MODE message to the seeker once RFCOMM
// is connected to ensure the correct state is propagated.
// Direction: Provider -> Seeker
// Handled by: Client app
#define MESSAGE_CODE_ENABLE_SILENCE_MODE 1

// Direction: Provider -> Seeker
// Handled by: Client app
#define MESSAGE_CODE_DISABLE_SILENCE_MODE 2

// Message group Companion App Event
#define MESSAGE_GROUP_COMPANION_APP_EVENT 2

// Direction: Provider -> Seeker
// Handled by: Client app
#define MESSAGE_CODE_LOG_BUFFER_FULL 1

// Message group Device Information Event
#define MESSAGE_GROUP_DEVICE_INFORMATION_EVENT 3

// Direction: Provider -> Seeker
// Handled by: Nearby Fast Pair library
#define MESSAGE_CODE_MODEL_ID 1

// Direction: Provider -> Seeker
// Handled by: Nearby Fast Pair library
#define MESSAGE_CODE_BLE_ADDRESS_UPDATED 2

// Direction: Provider -> Seeker
// Handled by: Nearby Fast Pair library
#define MESSAGE_CODE_BATTERY_UPDATED 3

// Direction: Provider -> Seeker
// Handled by: Nearby Fast Pair library
#define MESSAGE_CODE_REMAINING_BATTERY_TIME 4

// Direction: Seeker -> Provider
// Handled by: Client app
#define MESSAGE_CODE_ACTIVE_COMPONENT_REQUEST 5

// Direction: Provider -> Seeker
// Handled by: Client app
#define MESSAGE_CODE_ACTIVE_COMPONENT_RESPONSE 6

// Direction: Provider -> Seeker
// Handled by: Client app
#define MESSAGE_CODE_CAPABILITIES 7

#define MESSAGE_CODE_CAPABILITIES_COMPANION_APP_INSTALLED 0
#define MESSAGE_CODE_CAPABILITIES_SILENCE_MODE_SUPPORTED 1

// Direction: Seeker -> Provider
// Handled by: Client app
#define MESSAGE_CODE_PLATFORM_TYPE 8

// Direction: Provider -> Seeker
// Handled by: Nearby Fast Pair library
#define MESSAGE_CODE_SESSION_NONCE 0x0A

// Message group Device Action Event
#define MESSAGE_GROUP_DEVICE_ACTION_EVENT 4

// Direction: Seeker -> Provider
// Handled by: Client app
#define MESSAGE_CODE_RING 1

#define MESSAGE_CODE_RING_LEFT 0
#define MESSAGE_CODE_RING_RIGHT 1

// Payload for |kNearbyEventMessageStreamReceived| event. See
// https://developers.google.com/nearby/fast-pair/spec#MessageStream
typedef struct {
  // The peer BT address
  uint64_t peer_address;
  // Message group
  uint8_t message_group;
  // Message code
  uint8_t message_code;
  // If |length| is 0, then there's no additional data in the message
  size_t length;
  // Optional. Addtional message data
  uint8_t *data;
} nearby_event_MessageStreamReceived;

// Message group Smart Audio Source Switching
#define MESSAGE_GROUP_SASS 7

// Direction: Both
// Handled by: Nearby Fast Pair library
// Encrypted: No
// Signed: No
// ACK required: No
#define MESSAGE_CODE_SASS_GET_CAPABILITY 0x10

// Direction: Both
// Handled by: Nearby Fast Pair library
// Encrypted: No
// Signed: Yes
// ACK required: Yes
#define MESSAGE_CODE_SASS_NOTIFY_CAPABILITY 0x11

// Direction: Seeker -> Provider
// Handled by: Nearby Fast Pair library
// Encrypted: No
// Signed: Yes
// ACK required: Yes
#define MESSAGE_CODE_SASS_SET_MULTIPOINT_STATE 0x12

// Direction: Seeker -> Provider
// Handled by: Nearby Fast Pair library
// Encrypted: No
// Signed: Yes
// ACK required: Yes
#define MESSAGE_CODE_SASS_SET_SWITCHING_PREFERENCE 0x20

// Direction: Both
// Handled by: Nearby Fast Pair library
// Encrypted: No
// Signed: No
// ACK required: No
#define MESSAGE_CODE_SASS_GET_SWITCHING_PREFERENCE 0x21

// Direction: Both
// Handled by: Nearby Fast Pair library
// Encrypted: No
// Signed: Yes
// ACK required: Yes
#define MESSAGE_CODE_SASS_NOTIFY_SWITCHING_PREFERENCE 0x22

// Direction: Seeker -> Provider
// Handled by: Nearby Fast Pair library
// Encrypted: No
// Signed: Yes
// ACK required: Yes
#define MESSAGE_CODE_SASS_SWITCH_ACTIVE_AUDIO_SOURCE 0x30

// Direction: Seeker -> Provider
// Handled by: Nearby Fast Pair library
// Encrypted: No
// Signed: Yes
// ACK required: Yes
#define MESSAGE_CODE_SASS_SWITCH_BACK_AUDIO_SOURCE 0x31

// Direction: Provider -> Seeker
// Handled by: Nearby Fast Pair library
// Encrypted: No
// Signed: No
// ACK required: No
#define MESSAGE_CODE_SASS_NOTIFY_MULTIPOINT_SWITCH_EVENT 0x32

// Direction: Seeker -> Provider
// Handled by: Nearby Fast Pair library
// Encrypted: No
// Signed: No
// ACK required: No
#define MESSAGE_CODE_SASS_GET_CONNECTION_STATUS 0x33

// Direction: Provider -> Seeker
// Handled by: Nearby Fast Pair library
// Encrypted: Yes
// Signed: No
// ACK required: No
#define MESSAGE_CODE_SASS_NOTIFY_CONNECTION_STATUS 0x34

// Direction: Seeker -> Provider
// Handled by: Nearby Fast Pair library
// Encrypted: No
// Signed: Yes
// ACK required: Yes
#define MESSAGE_CODE_SASS_NOTIFY_SASS_INITIATED_CONNECTION 0x40

// Direction: Seeker -> Provider
// Handled by: Nearby Fast Pair library
// Encrypted: No
// Signed: Yes
// ACK required: Yes
#define MESSAGE_CODE_SASS_IN_USE_ACCOUNT_KEY 0x41

// Direction: Seeker -> Provider
// Handled by: Nearby Fast Pair library
// Encrypted: No
// Signed: Yes
// ACK required: Yes
#define MESSAGE_CODE_SASS_SEND_CUSTOM_DATA 0x42

// Direction: Seeker -> Provider
// Handled by: Nearby Fast Pair library
// Encrypted: No
// Signed: Yes
// ACK required: Yes
#define MESSAGE_CODE_SASS_SET_DROP_CONNECTION_TARGET 0x43

#endif /* NEARBY_EVENT_H */
