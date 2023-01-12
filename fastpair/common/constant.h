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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_COMMON_CONSTANT_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_COMMON_CONSTANT_H_

#include <stdint.h>

namespace nearby {
namespace fastpair {

// Bluetooth Uuid
constexpr char kServiceId[] = "Fast Pair";
constexpr char kFastPairServiceUuid[] = "0000FE2C-0000-1000-8000-00805F9B34FB";
const char kKeyBasedPairingCharacteristicUuidV1[] = "1234";
const char kKeyBasedPairingCharacteristicUuidV2[] =
    "FE2C1234-8366-4814-8EB0-01DE32100BEA";
const char kPasskeyCharacteristicUuidV1[] = "1235";
const char kPasskeyCharacteristicUuidV2[] =
    "FE2C1235-8366-4814-8EB0-01DE32100BEA";
const char kAccountKeyCharacteristicUuidV1[] = "1236";
const char kAccountKeyCharacteristicUuidV2[] =
    "FE2C1236-8366-4814-8EB0-01DE32100BEA";

// Key pair
constexpr int kSharedSecretKeyByteSize = 16;
constexpr int kPublicKeyByteSize = 64;

// Encryption
constexpr int kAesBlockByteSize = 16;
constexpr int kEncryptedDataByteSize = 16;
// Decryption
constexpr int kDecryptedResponseAddressByteSize = 6;
constexpr int kDecryptedResponseSaltByteSize = 9;
constexpr int kDecryptedPasskeySaltByteSize = 12;

// Handshake response index
constexpr int kMessageTypeIndex = 0;
constexpr int kResponseAddressStartIndex = 1;
constexpr int kResponseSaltStartIndex = 7;
constexpr int kPasskeySaltStartIndex = 4;

// Handshake response message type
constexpr uint8_t kKeybasedPairingResponseType = 0x01;
constexpr uint8_t kSeekerPasskeyType = 0x02;
constexpr uint8_t kProviderPasskeyType = 0x03;

// Handshake request inex
constexpr uint8_t kProviderAddressStartIndex = 2;
constexpr uint8_t kSeekerAddressStartIndex = 8;
constexpr uint8_t kSeekerPasskey = 0x02;
constexpr uint8_t kAccountKeyStartByte = 0x04;

// Handshake request message type
constexpr uint8_t kKeyBasedPairingType = 0x00;
constexpr uint8_t kInitialOrSubsequentFlags = 0x00;
constexpr uint8_t kRetroactiveFlags = 0x10;

// GATT connection
constexpr int kMaxNumGattConnectionAttempts = 3;
constexpr int kGattOperationTimeout = 15;

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_COMMON_CONSTANT_H_
