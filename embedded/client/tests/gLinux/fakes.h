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

// Copyright 2021 Google LLC.
#ifndef NEARBY_TEST_FAKES_H
#define NEARBY_TEST_FAKES_H
#include <map>
#include <queue>
#include <vector>

#include "nearby.h"
#include "nearby_platform_ble.h"

void nearby_test_fakes_SetRandomNumber(unsigned int value);
void nearby_test_fakes_SetRandomNumberSequence(std::vector<uint8_t>& value);

void nearby_test_fakes_SetAccountKeys(const uint8_t* input, size_t length);
std::vector<uint8_t> nearby_test_fakes_GetRawAccountKeys();

nearby_platform_status nearby_test_fakes_GattReadModelId(uint8_t* output,
                                                         size_t* length);

nearby_platform_status nearby_test_fakes_SetAntiSpoofingKey(
    const uint8_t private_key[32], const uint8_t public_key[64]);

nearby_platform_status nearby_test_fakes_GenSec256r1Secret(
    const uint8_t remote_party_public_key[64], uint8_t secret[32]);

nearby_platform_status nearby_test_fakes_Aes128Decrypt(
    const uint8_t input[AES_MESSAGE_SIZE_BYTES],
    uint8_t output[AES_MESSAGE_SIZE_BYTES],
    const uint8_t key[AES_MESSAGE_SIZE_BYTES]);
nearby_platform_status nearby_test_fakes_Aes128Encrypt(
    const uint8_t input[AES_MESSAGE_SIZE_BYTES],
    uint8_t output[AES_MESSAGE_SIZE_BYTES],
    const uint8_t key[AES_MESSAGE_SIZE_BYTES]);

std::vector<uint8_t>& nearby_test_fakes_GetAdvertisement();

std::map<nearby_fp_Characteristic, std::vector<uint8_t>>&
nearby_test_fakes_GetGattNotifications();

void nearby_test_fakes_SimulatePairing(uint64_t peer_address);
nearby_platform_status nearby_fp_fakes_ReceiveKeyBasedPairingRequest(
    const uint8_t* request, size_t length);
nearby_platform_status nearby_fp_fakes_ReceivePasskey(const uint8_t* request,
                                                      size_t length);
nearby_platform_status nearby_fp_fakes_ReceiveAccountKeyWrite(
    const uint8_t* request, size_t length);
nearby_platform_status nearby_fp_fakes_ReceiveAdditionalData(
    const uint8_t* request, size_t length);
uint64_t nearby_test_fakes_GetPairingRequestAddress();
uint32_t nearby_test_fakes_GetRemotePasskey();
uint64_t nearby_test_fakes_GetPairedDevice();
void nearby_test_fakes_DevicePaired(uint64_t peer_address);

class AccountKeyList {
 public:
  explicit AccountKeyList(const std::vector<uint8_t>& raw_values) {
    if (raw_values.size() == 0) return;
    int key_count = raw_values[0];
    for (int i = 0; i < key_count; i++) {
      const uint8_t* p = raw_values.data() + 1 + (i * ACCOUNT_KEY_SIZE_BYTES);
      keys_.emplace_back(std::vector<uint8_t>(p, p + ACCOUNT_KEY_SIZE_BYTES));
    }
  }

  size_t size() { return keys_.size(); }

  std::vector<std::vector<uint8_t>>& GetKeys() { return keys_; }

  std::vector<uint8_t> GetRawFormat() {
    std::vector<uint8_t> result;
    result.push_back(size());
    for (auto& key : keys_) {
      for (auto& v : key) {
        result.push_back(v);
      }
    }
    return result;
  }

 private:
  std::vector<std::vector<uint8_t>> keys_;
};

void nearby_test_fakes_SetAccountKeys(AccountKeyList& keys);

AccountKeyList nearby_test_fakes_GetAccountKeys();

void nearby_test_fakes_SetIsCharging(bool charging);

void nearby_test_fakes_SetRightBudBatteryLevel(unsigned battery_level);

void nearby_test_fakes_SetLeftBudBatteryLevel(unsigned battery_level);

void nearby_test_fakes_SetChargingCaseBatteryLevel(unsigned battery_level);

void nearby_test_fakes_BatteryTime(uint16_t battery_time);

void nearby_test_fakes_SetGetBatteryInfoResult(nearby_platform_status status);

std::vector<uint8_t>& nearby_test_fakes_GetRfcommOutput();

void nearby_test_fakes_MessageStreamConnected(uint64_t peer_address);

void nearby_test_fakes_MessageStreamDisconnected(uint64_t peer_address);

void nearby_test_fakes_MessageStreamReceived(uint64_t peer_address,
                                             const uint8_t* message,
                                             size_t length);

uint32_t nearby_test_fakes_GetNextTimerMs();
// Sets current time and triggers the timer callback if it expired
void nearby_test_fakes_SetCurrentTimeMs(uint32_t ms);

void nearby_test_fakes_SetInPairingMode(bool in_pairing_mode);

uint8_t nearby_test_fakes_GetRingCommand(void);

uint16_t nearby_test_fakes_GetRingTimeout(void);

#endif /* NEARBY_TEST_FAKES_H */
