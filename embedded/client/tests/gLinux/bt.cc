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

#include <cstring>
#include <map>
#include <vector>

#include "nearby_platform_bt.h"

static const uint32_t kFastPairId = 0x101112;
static const int8_t kTxLevel = 33;
static const uint64_t kPublicAddress = 0xA0A1A2A3A4A5;
// No secondary identity address by default.
static const uint64_t kSecondaryPublicAddress = 0;
static const uint32_t kLocalPasskey = 123456;
static uint32_t remote_passkey;
static uint64_t remote_address;
static uint64_t paired_peer_address;
static std::map<uint64_t, std::vector<uint8_t>> rfcomm_outputs;
static std::vector<char> device_name;
static bool pairing_mode = false;
static uint64_t secondary_public_address = kSecondaryPublicAddress;

static const nearby_platform_BtInterface* bt_interface;
// Returns Fast Pair Model Id.
uint32_t nearby_platform_GetModelId() { return kFastPairId; }

int8_t nearby_platform_GetTxLevel() { return kTxLevel; }

// Returns public BR/EDR address
uint64_t nearby_platform_GetPublicAddress() { return kPublicAddress; }

uint64_t nearby_platform_GetSecondaryPublicAddress() {
  return secondary_public_address;
}

// Returns passkey used during pairing
uint32_t nearby_platfrom_GetPairingPassKey() { return kLocalPasskey; }

void nearby_platform_SetRemotePasskey(uint32_t passkey) {
  remote_passkey = passkey;
  if (remote_passkey == kLocalPasskey &&
      remote_address != 0 & bt_interface != NULL) {
    paired_peer_address = remote_address;
    bt_interface->on_paired(remote_address);
  }
}

nearby_platform_status nearby_platform_SendPairingRequest(
    uint64_t remote_party_br_edr_address) {
  remote_address = remote_party_br_edr_address;
  return kNearbyStatusOK;
}

nearby_platform_status nearby_platform_SetFastPairCapabilities() {
  return kNearbyStatusOK;
}

nearby_platform_status nearby_platform_SetDefaultCapabilities() {
  remote_address = 0;
  return kNearbyStatusOK;
}

nearby_platform_status nearby_platform_BtInit(
    const nearby_platform_BtInterface* callbacks) {
  bt_interface = callbacks;
  remote_address = 0;
  remote_passkey = 0;
  paired_peer_address = 0;
  pairing_mode = false;
  secondary_public_address = kSecondaryPublicAddress;
  rfcomm_outputs.clear();
  return kNearbyStatusOK;
}

uint64_t nearby_test_fakes_GetPairingRequestAddress() { return remote_address; }

uint32_t nearby_test_fakes_GetRemotePasskey() { return remote_passkey; }

void nearby_test_fakes_SimulatePairing(uint64_t peer_address) {
  remote_address = peer_address;
  if (bt_interface != NULL) {
    bt_interface->on_pairing_request(peer_address);
  }
}

void nearby_test_fakes_SetSecondaryPublicAddress(uint64_t address) {
  secondary_public_address = address;
}

uint64_t nearby_test_fakes_GetPairedDevice() { return paired_peer_address; }

void nearby_test_fakes_DevicePaired(uint64_t peer_address) {
  bt_interface->on_paired(peer_address);
}

nearby_platform_status nearby_platform_SendMessageStream(uint64_t peer_address,
                                                         const uint8_t* message,
                                                         size_t length) {
  rfcomm_outputs.emplace(peer_address, std::vector<uint8_t>());
  auto rfcomm_output = rfcomm_outputs.find(peer_address);
  for (int i = 0; i < length; i++) {
    rfcomm_output->second.push_back(message[i]);
  }
  return kNearbyStatusOK;
}

std::vector<uint8_t>& nearby_test_fakes_GetRfcommOutput(uint64_t peer_address) {
  return rfcomm_outputs[peer_address];
}

std::vector<uint8_t>& nearby_test_fakes_GetRfcommOutput() {
  return nearby_test_fakes_GetRfcommOutput(paired_peer_address);
}

nearby_platform_status nearby_platform_SetDeviceName(const char* name) {
  // + 1 for null terminator
  device_name = std::vector<char>(name, name + std::strlen(name) + 1);
  return kNearbyStatusOK;
}

// Gets null-terminated device name string in UTF-8 encoding
// pass buffer size in char, and get string length in char.
nearby_platform_status nearby_platform_GetDeviceName(char* name,
                                                     size_t* length) {
  if (*length < device_name.size()) {
    return kNearbyStatusResourceExhausted;
  }
  *length = device_name.size();
  std::memcpy(name, device_name.data(), device_name.size());
  return kNearbyStatusOK;
}

bool nearby_platform_IsInPairingMode() { return pairing_mode; }

void nearby_test_fakes_SetInPairingMode(bool in_pairing_mode) {
  pairing_mode = in_pairing_mode;
}

#if NEARBY_FP_MESSAGE_STREAM
void nearby_test_fakes_MessageStreamConnected(uint64_t peer_address) {
  bt_interface->on_message_stream_connected(peer_address);
}

void nearby_test_fakes_MessageStreamDisconnected(uint64_t peer_address) {
  bt_interface->on_message_stream_disconnected(peer_address);
}

void nearby_test_fakes_MessageStreamReceived(uint64_t peer_address,
                                             const uint8_t* message,
                                             size_t length) {
  bt_interface->on_message_stream_received(peer_address, message, length);
}
#endif /* NEARBY_FP_MESSAGE_STREAM */
