// Copyright 2023 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_HANDSHAKE_FAST_PAIR_HANDSHAKE_IMPL_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_HANDSHAKE_FAST_PAIR_HANDSHAKE_IMPL_H_

#include <memory>
#include <optional>

#include "fastpair/common/fast_pair_device.h"
#include "fastpair/common/pair_failure.h"
#include "fastpair/crypto/decrypted_response.h"
#include "fastpair/handshake/fast_pair_handshake.h"
#include "fastpair/internal/mediums/mediums.h"

namespace nearby {
namespace fastpair {

class FastPairHandshakeImpl : public FastPairHandshake {
 public:
  explicit FastPairHandshakeImpl(FastPairDevice& device, Mediums& mediums,
                                 OnCompleteCallback on_complete,
                                 SingleThreadExecutor* executor);
  FastPairHandshakeImpl(const FastPairHandshakeImpl&) = delete;
  FastPairHandshakeImpl& operator=(const FastPairHandshakeImpl&) = delete;

 private:
  void OnGattClientInitializedCallback(FastPairDevice& device,
                                       std::optional<PairFailure> failure);
  void OnDataEncryptorCreateAsync(
      FastPairDevice& device,
      std::unique_ptr<FastPairDataEncryptor> fast_pair_data_encryptor);
  void OnWriteResponse(FastPairDevice& device, absl::string_view response,
                       std::optional<PairFailure> failure);
  void OnParseDecryptedResponse(FastPairDevice& device,
                                std::optional<DecryptedResponse>& response);
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_HANDSHAKE_FAST_PAIR_HANDSHAKE_IMPL_H_
