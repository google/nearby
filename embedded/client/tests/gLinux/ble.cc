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

#include <map>
#include <vector>

// clang-format off
#include "nearby_config.h"
// clang-format on

#include "fakes.h"
#include "nearby_platform_ble.h"
#include "nearby_platform_se.h"

constexpr uint64_t kDefaultBleAddress = 0xbabababa;
static uint64_t ble_address = kDefaultBleAddress;
static uint64_t peer_address = 0x345678ab;
static const nearby_platform_BleInterface* ble_interface;
static std::map<nearby_fp_Characteristic, std::vector<uint8_t>> notifications;
static std::vector<uint8_t> advertisement;
static nearby_fp_AvertisementInterval interval;
constexpr int32_t kDefaultPsm = -1;
static int32_t psm = kDefaultPsm;

std::map<nearby_fp_Characteristic, std::vector<uint8_t>>&
nearby_test_fakes_GetGattNotifications() {
  return notifications;
}

// Gets BLE address.
uint64_t nearby_platform_GetBleAddress() { return ble_address; }

// Sets BLE address. Returns address after change, which may be different than
// requested address.
uint64_t nearby_platform_SetBleAddress(uint64_t address) {
  ble_address = address;
  return ble_address;
}

#ifdef NEARBY_FP_HAVE_BLE_ADDRESS_ROTATION
// Rotates BLE address to a random resolvable private address (RPA). Returns
// address after change.
uint64_t nearby_platform_RotateBleAddress() {
  unsigned i;
  uint64_t address = 0;
  for (i = 0; i < sizeof(address); i++) {
    address = (address << 8) ^ nearby_platform_Rand();
  }
  address |= (uint64_t)1 << 46;
  address &= ~((uint64_t)1 << 47);
  nearby_platform_SetBleAddress(address);
  return address;
}
#endif /* NEARBY_FP_HAVE_BLE_ADDRESS_ROTATION */

// Sends a notification to the connected GATT client.
nearby_platform_status nearby_platform_GattNotify(
    uint64_t peer_address, nearby_fp_Characteristic characteristic,
    const uint8_t* message, size_t length) {
  notifications.emplace(characteristic,
                        std::vector<uint8_t>(message, message + length));
  return kNearbyStatusOK;
}

nearby_platform_status nearby_platform_SetAdvertisement(
    const uint8_t* payload, size_t length,
    nearby_fp_AvertisementInterval interval) {
  if (payload == NULL || length == 0) {
    advertisement.clear();
  } else {
    advertisement.assign(payload, payload + length);
  }
  ::interval = interval;
  return kNearbyStatusOK;
}

int32_t nearby_platform_GetMessageStreamPsm() { return psm; }

// Initializes BLE
nearby_platform_status nearby_platform_BleInit(
    const nearby_platform_BleInterface* callbacks) {
  ble_interface = callbacks;
  notifications.clear();
  advertisement.clear();
  interval = kDisabled;
  ble_address = kDefaultBleAddress;
  psm = kDefaultPsm;
  return kNearbyStatusOK;
}

void nearby_test_fakes_SetPsm(int32_t value) { psm = value; }

std::vector<uint8_t>& nearby_test_fakes_GetAdvertisement() {
  return advertisement;
}

nearby_platform_status nearby_test_fakes_GattReadModelId(uint8_t* output,
                                                         size_t* length) {
  return ble_interface->on_gatt_read(peer_address, kModelId, output, length);
}

nearby_platform_status nearby_fp_fakes_ReceiveKeyBasedPairingRequest(
    const uint8_t* request, size_t length) {
  return ble_interface->on_gatt_write(peer_address, kKeyBasedPairing, request,
                                      length);
}

nearby_platform_status nearby_fp_fakes_ReceivePasskey(const uint8_t* request,
                                                      size_t length) {
  return ble_interface->on_gatt_write(peer_address, kPasskey, request, length);
}

nearby_platform_status nearby_fp_fakes_ReceiveAccountKeyWrite(
    const uint8_t* request, size_t length) {
  return ble_interface->on_gatt_write(peer_address, kAccountKey, request,
                                      length);
}

nearby_platform_status nearby_fp_fakes_ReceiveAdditionalData(
    const uint8_t* request, size_t length) {
  return ble_interface->on_gatt_write(peer_address, kAdditionalData, request,
                                      length);
}

nearby_platform_status nearby_fp_fakes_GattReadMessageStreamPsm(
    uint8_t* output, size_t* length) {
  return ble_interface->on_gatt_read(peer_address, kMessageStreamPsm, output,
                                     length);
}
