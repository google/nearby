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

std::vector<uint8_t> nearby_test_fakes_GetRawAccountKeys();

nearby_platform_status nearby_test_fakes_GattReadModelId(uint8_t* output,
                                                         size_t* length);

#ifdef MBEDTLS_FOR_SSL
extern "C" {
#endif
nearby_platform_status nearby_test_fakes_SetAntiSpoofingKey(
    const uint8_t private_key[32], const uint8_t public_key[64]);
#ifdef MBEDTLS_FOR_SSL
}
#endif

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
nearby_platform_status nearby_fp_fakes_GattReadMessageStreamPsm(uint8_t* output,
                                                                size_t* length);
uint64_t nearby_test_fakes_GetPairingRequestAddress();
uint32_t nearby_test_fakes_GetRemotePasskey();
uint64_t nearby_test_fakes_GetPairedDevice();
void nearby_test_fakes_DevicePaired(uint64_t peer_address);

class AccountKeyPair {
 public:
  AccountKeyPair(uint64_t bt_address, const std::vector<uint8_t>& account_key)
      : address_(bt_address), account_key_(account_key) {}
  AccountKeyPair(uint64_t bt_address, const uint8_t* account_key)
      : address_(bt_address),
        account_key_(account_key, account_key + ACCOUNT_KEY_SIZE_BYTES) {}

  uint64_t address_;
  std::vector<uint8_t> account_key_;
};

struct RawAccountKeyList {
  uint8_t num_keys;
  nearby_platform_AccountKeyInfo key[NEARBY_MAX_ACCOUNT_KEYS];
};

class AccountKeyList {
 public:
  explicit AccountKeyList(const std::vector<uint8_t>& raw_values) {
    const RawAccountKeyList* list =
        reinterpret_cast<const RawAccountKeyList*>(raw_values.data());
    if (!list) return;

    for (size_t i = 0; i < list->num_keys; i++) {
      uint64_t address = 0;
#ifdef NEARBY_FP_ENABLE_SASS
      address = list->key[i].peer_address;
#endif /* NEARBY_FP_ENABLE_SASS */
      key_pairs_.emplace_back(
          address, std::vector<uint8_t>(
                       list->key[i].account_key,
                       list->key[i].account_key + ACCOUNT_KEY_SIZE_BYTES));
    }
  }
  explicit AccountKeyList(const std::vector<AccountKeyPair>& key_pairs)
      : key_pairs_(key_pairs) {}

  size_t size() { return key_pairs_.size(); }

  std::vector<uint8_t> GetKey(int index) {
    return key_pairs_[index].account_key_;
  }

  std::vector<uint8_t> GetRawFormat() {
    RawAccountKeyList account_keys;
    account_keys.num_keys = size();
    for (size_t i = 0; i < size(); i++) {
#ifdef NEARBY_FP_ENABLE_SASS
      account_keys.key[i].peer_address = key_pairs_[i].address_;
#endif /* NEARBY_FP_ENABLE_SASS */
      for (size_t j = 0; j < key_pairs_[i].account_key_.size(); j++) {
        account_keys.key[i].account_key[j] = key_pairs_[i].account_key_[j];
      }
    }
    uint8_t* p = reinterpret_cast<uint8_t*>(&account_keys);
    return std::vector<uint8_t>(p, p + sizeof(account_keys));
  }

 private:
  std::vector<AccountKeyPair> key_pairs_;
};

void nearby_test_fakes_SetAccountKeys(const uint8_t* input, size_t length);
void nearby_test_fakes_SetAccountKeys(
    std::vector<AccountKeyPair>& account_key_pairs);
void nearby_test_fakes_SetAccountKeys(AccountKeyList& keys);

AccountKeyList nearby_test_fakes_GetAccountKeys();

void nearby_test_fakes_SetIsCharging(bool charging);

void nearby_test_fakes_SetRightBudBatteryLevel(unsigned battery_level);

void nearby_test_fakes_SetLeftBudBatteryLevel(unsigned battery_level);

void nearby_test_fakes_SetChargingCaseBatteryLevel(unsigned battery_level);

void nearby_test_fakes_BatteryTime(uint16_t battery_time);

void nearby_test_fakes_SetGetBatteryInfoResult(nearby_platform_status status);

std::vector<uint8_t>& nearby_test_fakes_GetRfcommOutput();
std::vector<uint8_t>& nearby_test_fakes_GetRfcommOutput(uint64_t peer_address);

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
uint64_t nearby_test_fakes_GetSassSwitchActiveSourcePeerAddress();
uint8_t nearby_test_fakes_GetSassSwitchActiveSourceFlags();
uint64_t nearby_test_fakes_GetSassSwitchActiveSourcePreferredAudioSource();
uint64_t nearby_test_fakes_GetSassSwitchBackPeerAddress();
uint8_t nearby_test_fakes_GetSassSwitchBackFlags();
uint64_t nearby_test_fakes_GetSassNotifyInitiatedConnectionPeerAddress();
uint8_t nearby_test_fakes_GetSassNotifyInitiatedConnectionFlags();
uint64_t nearby_test_fakes_GetSassDropConnectionTargetPeerAddress();
uint8_t nearby_test_fakes_GetSassDropConnectionTargetFlags();
void nearby_test_fakes_SassMultipointSwitch(uint8_t reason,
                                            uint64_t peer_address,
                                            const char* name);

#ifdef NEARBY_FP_ENABLE_SASS
void nearby_test_fakes_SetActiveAudioSource(uint64_t peer_address);
#endif /* NEARBY_FP_ENABLE_SASS */

void nearby_test_fakes_SetPsm(int32_t value);

void nearby_test_fakes_SetSecondaryPublicAddress(uint64_t address);

#endif /* NEARBY_TEST_FAKES_H */
