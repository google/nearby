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

#ifndef NEARBY_PLATFORM_BT_H
#define NEARBY_PLATFORM_BT_H

// clang-format off
#include "nearby_config.h"
// clang-format on

#include "nearby.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct {
  void (*on_pairing_request)(uint64_t peer_address);
  void (*on_paired)(uint64_t peer_address);
  void (*on_pairing_failed)(uint64_t peer_address);
#if NEARBY_FP_MESSAGE_STREAM
  // Message Stream messages are sent over RFCOMM on BT devices or L2CAP on BLE
  // devices.
  void (*on_message_stream_connected)(uint64_t peer_address);
  void (*on_message_stream_disconnected)(uint64_t peer_address);
  void (*on_message_stream_received)(uint64_t peer_address,
                                     const uint8_t* message, size_t length);
#endif /* NEARBY_FP_MESSAGE_STREAM */
} nearby_platform_BtInterface;

// Returns Fast Pair Model Id.
uint32_t nearby_platform_GetModelId();

// Returns tx power level.
int8_t nearby_platform_GetTxLevel();

// Returns public BR/EDR address.
// On a BLE-only device, return the public identity address.
uint64_t nearby_platform_GetPublicAddress();

// Returns the secondary identity address.
// Some devices, such as ear-buds, can advertise two identity addresses. In this
// case, the Seeker pairs with each address separately but treats them as a
// single logical device set.
// Return 0 if this device does not have a secondary identity address.
uint64_t nearby_platform_GetSecondaryPublicAddress();

// Returns passkey used during pairing
uint32_t nearby_platfrom_GetPairingPassKey();

// Provides the passkey received from the remote party.
// The system should compare local and remote party and accept/decline pairing
// request accordingly.
//
// passkey - Passkey
void nearby_platform_SetRemotePasskey(uint32_t passkey);

// Sends a pairing request to the Seeker
//
// remote_party_br_edr_address - BT address of peer.
nearby_platform_status nearby_platform_SendPairingRequest(
    uint64_t remote_party_br_edr_address);

// Switches the device capabilities field back to default so that new
// pairings continue as expected.
nearby_platform_status nearby_platform_SetDefaultCapabilities();

// Switches the device capabilities field to Fast Pair required configuration:
// DisplayYes/No so that `confirm passkey` pairing method will be used.
nearby_platform_status nearby_platform_SetFastPairCapabilities();

// Sets null-terminated device name string in UTF-8 encoding
//
// name - Zero terminated string name of device.
nearby_platform_status nearby_platform_SetDeviceName(const char* name);

// Gets null-terminated device name string in UTF-8 encoding
// pass buffer size in char, and get string length in char.
//
// name   - Buffer to return name string.
// length - On input, the size of the name buffer.
//          On output, returns size of name in buffer.
nearby_platform_status nearby_platform_GetDeviceName(char* name,
                                                     size_t* length);

// Returns true if the device is in pairing mode (either fast-pair or manual).
bool nearby_platform_IsInPairingMode();

#if NEARBY_FP_MESSAGE_STREAM
// Sends message stream through RFCOMM or L2CAP channel initiated by Seeker.
// BT devices should use RFCOMM channel. BLE-only devices should use L2CAP.
//
// peer_address - Peer address.
// message      - Contents of message.
// length       - Length of message
nearby_platform_status nearby_platform_SendMessageStream(uint64_t peer_address,
                                                         const uint8_t* message,
                                                         size_t length);
#endif /* NEARBY_FP_MESSAGE_STREAM */
// Initializes BT module
//
// bt_interface - BT callbacks event structure.
nearby_platform_status nearby_platform_BtInit(
    const nearby_platform_BtInterface* bt_interface);

#ifdef __cplusplus
}
#endif

#endif /* NEARBY_PLATFORM_BT_H */
