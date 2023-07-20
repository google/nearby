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
  kUnknown = 0,
  // Failed to create a GATT connection to the device.
  kCreateGattConnection = 1,
  // Failed to find the expected GATT service.
  kGattServiceDiscovery = 2,
  // Timed out while starting discovery of GATT service.
  kGattServiceDiscoveryTimeout = 3,
  // Failed to retrieve the data encryptor.
  kDataEncryptorRetrieval = 4,
  // Failed to find the Key-based pairing GATT characteristic.
  kKeyBasedPairingCharacteristicDiscovery = 5,
  // Failed to find the Passkey GATT characteristic.
  kPasskeyCharacteristicDiscovery = 6,
  // Failed to find the Account Key GATT characteristic.
  kAccountKeyCharacteristicDiscovery = 7,
  // Failed to subscribe the notification on the Key-based pairing GATT
  // characteristic.
  kKeyBasedPairingCharacteristicSubscription = 8,
  // Failed to start a notify session on the Passkey GATT characteristic.
  kPasskeyCharacteristicSubscription = 9,
  // Timed out while waiting to start a notify session on the Key-based pairing
  // GATT characteristic.
  kKeyBasedPairingCharacteristicSubscriptionTimeout = 10,
  // / Timed out while waiting to start a notify session on the Passkey GATT
  // characteristic.
  kPasskeyCharacteristicSubscriptionTimeout = 11,
  // Failed to write to the Key-based pairing GATT characteristic.
  kKeyBasedPairingCharacteristicWrite = 12,
  // Failed to write to the Passkey GATT characteristic.
  kPasskeyPairingCharacteristicWrite = 13,
  // Failed to write to the AccountKey GATT characteristic.
  kAccountKeyCharacteristicWrite = 14,
  // Timed out while waiting for the Key-based Pairing response.
  kKeyBasedPairingResponseTimeout = 15,
  // Timed out while waiting for the Passkey response.
  kPasskeyResponseTimeout = 16,
  // Failed to decrypt Key-based response message.
  kKeybasedPairingResponseDecryptFailure = 17,
  // Incorrect Key-based response message type.
  kIncorrectKeyBasedPairingResponseType = 18,
  // Failed to decrypt Passkey response message.
  kPasskeyDecryptFailure = 19,
  // Incorrect Passkey response message type.
  kIncorrectPasskeyResponseType = 20,
  // Passkeys did not match.
  kPasskeyMismatch = 21,
  // Potential pairing device lost between GATT connection attempts.
  kPairingDeviceLostBetweenGattConnectionAttempts = 22,
  // Potential pairing device lost during pairing.
  kDeviceLostMidPairing = 23,
  // Failed to pair and connect with discovered device.
  kPairingAndConnect = 24,
  // Potential pairing timeout.
  kPairingTimeout = 25,
  kWriteAccountKeyToFootprints = 26,
  kMaxValue = kWriteAccountKeyToFootprints,
};

std::ostream& operator<<(std::ostream& stream, PairFailure failure);

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_COMMON_PAIR_FAILURE_H_
