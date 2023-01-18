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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_CRYPTO_FAST_PAIR_MESSAGE_TYPE_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_CRYPTO_FAST_PAIR_MESSAGE_TYPE_H_

namespace nearby {
namespace fastpair {

// Type values for Fast Pair messages.
enum class FastPairMessageType {
  // Key-based Pairing Request.
  kKeyBasedPairingRequest,
  // Key-based Pairing Response.
  kKeyBasedPairingResponse,
  // Seeker's passkey.
  kSeekersPasskey,
  // Provider's passkey.
  kProvidersPasskey,
  // Unknown message type.
  kUnknown,
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_CRYPTO_FAST_PAIR_MESSAGE_TYPE_H_
