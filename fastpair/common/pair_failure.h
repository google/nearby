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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_COMMON_PAIR_FAILURE_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_COMMON_PAIR_FAILURE_H_

#include <ostream>

namespace nearby {
namespace fastpair {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PairFailure {
  // Failed to create a GATT connection to the device.
  kCreateGattConnection = 0,
  // Failed to find the expected GATT service.
  kGattServiceDiscovery = 1,
  // Timed out while starting discovery of GATT service.
  kGattServiceDiscoveryTimeout = 2,
  // Failed to retrieve the data encryptor.
  kDataEncryptorRetrieval = 3,
  // Failed to find the Key-based pairing GATT characteristic.
  kKeyBasedPairingCharacteristicDiscovery = 4,
  // Failed to find the Passkey GATT characteristic.
  kPasskeyCharacteristicDiscovery = 5,
  // Failed to find the Account Key GATT characteristic.
  kAccountKeyCharacteristicDiscovery = 6,
  // Failed to subscribe the notification on the Key-based pairing GATT
  // characteristic.
  kKeyBasedPairingCharacteristicSubscription = 7,
  // Failed to start a notify session on the Passkey GATT characteristic.
  kPasskeyCharacteristicSubscription = 8,
  // Timed out while waiting to start a notify session on the Key-based pairing
  // GATT characteristic.
  kKeyBasedPairingCharacteristicSubscriptionTimeout = 9,
  // / Timed out while waiting to start a notify session on the Passkey GATT
  // characteristic.
  kPasskeyCharacteristicSubscriptionTimeout = 10,
  // Failed to write to the Key-based pairing GATT characteristic.
  kKeyBasedPairingCharacteristicWrite = 11,
  // Failed to write to the Passkey GATT characteristic.
  kPasskeyPairingCharacteristicWrite = 12,
  // Timed out while waiting for the Key-based Pairing response.
  kKeyBasedPairingResponseTimeout = 13,
  // Timed out while waiting for the Passkey response.
  kPasskeyResponseTimeout = 14,
  // Failed to decrypt Key-based response message.
  kKeybasedPairingResponseDecryptFailure = 15,
  // Incorrect Key-based response message type.
  kIncorrectKeyBasedPairingResponseType = 16,
  // Failed to decrypt Passkey response message.
  kPasskeyDecryptFailure = 17,
  // Incorrect Passkey response message type.
  kIncorrectPasskeyResponseType = 18,
  // Passkeys did not match.
  kPasskeyMismatch = 19,
  // Potential pairing device lost between GATT connection attempts.
  kPairingDeviceLostBetweenGattConnectionAttempts = 20,
  // Potential pairing device lost during pairing.
  kDeviceLostMidPairing = 21,
  // Failed to pair and connect with discovered device.
  kPairingAndConnect = 22,
  // Potential pairing timeout.
  kPairingTimeout = 23,
  kMaxValue = kPairingTimeout,
};

std::ostream& operator<<(std::ostream& stream, PairFailure failure);

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_COMMON_PAIR_FAILURE_H_
