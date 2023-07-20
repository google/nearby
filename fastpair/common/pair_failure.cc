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

#include "fastpair/common/pair_failure.h"

#include <ostream>

namespace nearby {
namespace fastpair {

std::ostream& operator<<(std::ostream& stream, PairFailure failure) {
  switch (failure) {
    case PairFailure::kUnknown:
      stream << "[Failed with unknown reason]";
      break;
    case PairFailure::kCreateGattConnection:
      stream << "[Failed to create a GATT connection to the device]";
      break;
    case PairFailure::kGattServiceDiscovery:
      stream << "[Failed to find the expected GATT service]";
      break;
    case PairFailure::kGattServiceDiscoveryTimeout:
      stream << "[Timed out while starting discovery of GATT service]";
      break;
    case PairFailure::kDataEncryptorRetrieval:
      stream << "[Failed to retrieve the data encryptor]";
      break;
    case PairFailure::kKeyBasedPairingCharacteristicDiscovery:
      stream << "[Failed to find the Key-based pairing GATT characteristic]";
      break;
    case PairFailure::kPasskeyCharacteristicDiscovery:
      stream << "[Failed to find the Passkey GATT characteristic]";
      break;
    case PairFailure::kAccountKeyCharacteristicDiscovery:
      stream << "[Failed to find the Account Key GATT characteristic]";
      break;
    case PairFailure::kKeyBasedPairingCharacteristicSubscription:
      stream << "[Failed to start a subscription on the Key-based pairing "
                "GATT characteristic]";
      break;
    case PairFailure::kPasskeyCharacteristicSubscription:
      stream << "[Failed to start a subscription on the Passkey GATT "
                "characteristic]";
      break;
    case PairFailure::kKeyBasedPairingCharacteristicSubscriptionTimeout:
      stream << "[Timed out while starting a subscription on the Key-based "
                "pairing GATT characteristic]";
      break;
    case PairFailure::kPasskeyCharacteristicSubscriptionTimeout:
      stream << "[Timed out while starting a subscription on the Passkey "
                "GATT characteristic]";
      break;
    case PairFailure::kKeyBasedPairingCharacteristicWrite:
      stream
          << "[Failed to write to the Key-based pairing GATT characteristic]";
      break;
    case PairFailure::kPasskeyPairingCharacteristicWrite:
      stream << "[Failed to write to the Passkey GATT characteristic]";
      break;
    case PairFailure::kAccountKeyCharacteristicWrite:
      stream << "[Failed to write to the Account Key GATT characteristic]";
      break;
    case PairFailure::kKeyBasedPairingResponseTimeout:
      stream << "[Timed out while waiting for the Key-based Pairing response]";
      break;
    case PairFailure::kPasskeyResponseTimeout:
      stream << "[Timed out while waiting for the Passkey response]";
      break;
    case PairFailure::kKeybasedPairingResponseDecryptFailure:
      stream << "[Failed to decrypt Key-based Pairing response]";
      break;
    case PairFailure::kIncorrectKeyBasedPairingResponseType:
      stream << "[Incorrect Key-based response message type]";
      break;
    case PairFailure::kPasskeyDecryptFailure:
      stream << "[Failed to decrypt Passkey response]";
      break;
    case PairFailure::kIncorrectPasskeyResponseType:
      stream << "[Incorrect Passkey response message type]";
      break;
    case PairFailure::kPasskeyMismatch:
      stream << "[Passkeys did not match]";
      break;
    case PairFailure::kPairingDeviceLostBetweenGattConnectionAttempts:
      stream
          << "[Potential pairing device lost between GATT connection attempts]";
      break;
    case PairFailure::kDeviceLostMidPairing:
      stream << "[Potential pairing device lost during pairing.]";
      break;
    case PairFailure::kPairingAndConnect:
      stream << "[Failed to pair with discovered device.]";
      break;
    case PairFailure::kPairingTimeout:
      stream << "[Potential pairing failed with timeout.]";
      break;
    case PairFailure::kWriteAccountKeyToFootprints:
      stream << "[Failed to write Account Key to Footprints.]";
      break;
  }

  return stream;
}

}  // namespace fastpair
}  // namespace nearby
