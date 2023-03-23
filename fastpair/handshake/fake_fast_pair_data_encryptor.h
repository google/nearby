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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_HANDSHAKE_FAKE_FAST_PAIR_DATA_ENCRYPTOR_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_HANDSHAKE_FAKE_FAST_PAIR_DATA_ENCRYPTOR_H_

#include <array>
#include <optional>
#include <utility>
#include <vector>

#include "fastpair/common/constant.h"
#include "fastpair/handshake/fast_pair_data_encryptor.h"

namespace nearby {
namespace fastpair {

class FakeFastPairDataEncryptor : public FastPairDataEncryptor {
 public:
  FakeFastPairDataEncryptor() = default;

  void SetPublickey(std::optional<std::array<uint8_t, 64>> public_key) {
    public_key_ = std::move(public_key);
  }

  void SetEncryptedBytes(
      std::array<uint8_t, kAesBlockByteSize> encrypted_bytes) {
    encrypted_bytes_ = std::move(encrypted_bytes);
  }

  void SetResponse(std::optional<DecryptedResponse> response) {
    response_ = std::move(response);
  }

  void SetPasskey(std::optional<DecryptedPasskey> passkey) {
    passkey_ = std::move(passkey);
  }

  // FastPairDataEncryptor:
  std::array<uint8_t, kAesBlockByteSize> EncryptBytes(
      const std::array<uint8_t, kAesBlockByteSize>& bytes_to_encrypt)
      const override {
    return encrypted_bytes_;
  }

  std::optional<std::array<uint8_t, 64>> GetPublicKey() const override {
    return public_key_;
  }

  void ParseDecryptResponse(
      const std::vector<uint8_t>& encrypted_response_bytes,
      absl::AnyInvocable<void(const std::optional<DecryptedResponse>)> callback)
      override {
    callback(response_);
  }

  void ParseDecryptPasskey(
      const std::vector<uint8_t>& encrypted_passkey_bytes,
      absl::AnyInvocable<void(const std::optional<DecryptedPasskey>)> callback)
      override {
    callback(passkey_);
  }

 private:
  std::optional<std::array<uint8_t, 64>> public_key_;
  std::array<uint8_t, kAesBlockByteSize> encrypted_bytes_;
  std::optional<DecryptedResponse> response_;
  std::optional<DecryptedPasskey> passkey_;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_HANDSHAKE_FAKE_FAST_PAIR_DATA_ENCRYPTOR_H_
