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

#include "fastpair/crypto/fast_pair_message_type.h"

#include <ostream>

namespace nearby {
namespace fastpair {

std::ostream& operator<<(std::ostream& stream,
                         FastPairMessageType message_type) {
  switch (message_type) {
    case FastPairMessageType::kKeyBasedPairingRequest:
      stream << "[Key-Based Pairing Request]";
      break;
    case FastPairMessageType::kKeyBasedPairingResponse:
      stream << "[Key-Based Pairing Response]";
      break;
    case FastPairMessageType::kSeekersPasskey:
      stream << "[Seeker's Passkey]";
      break;
    case FastPairMessageType::kProvidersPasskey:
      stream << "[Providers' Passkey]";
      break;
    default:
      stream << "[Unknown]";
  }

  return stream;
}

}  // namespace fastpair
}  // namespace nearby
