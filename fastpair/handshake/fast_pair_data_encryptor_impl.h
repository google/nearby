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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_HANDSHAKE_FAST_PAIR_DATA_ENCRYPTOR_IMPL_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_HANDSHAKE_FAST_PAIR_DATA_ENCRYPTOR_IMPL_H_

#include <stdint.h>

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "fastpair/common/constant.h"
#include "fastpair/common/device_metadata.h"
#include "fastpair/common/fast_pair_device.h"
#include "fastpair/crypto/fast_pair_key_pair.h"
#include "fastpair/handshake/fast_pair_data_encryptor.h"

namespace nearby {
namespace fastpair {

// Holds a secret key for a device and has methods to encrypt bytes, decrypt
// response and decrypt passkey.
class FastPairDataEncryptorImpl : public FastPairDataEncryptor {
 public:
  class Factory {
   public:
    static void CreateAsync(
        const FastPairDevice& device,
        absl::AnyInvocable<void(std::unique_ptr<FastPairDataEncryptor>)>
            on_get_instance_callback);

    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();

    virtual void CreateInstance(
        const FastPairDevice& device,
        absl::AnyInvocable<void(std::unique_ptr<FastPairDataEncryptor>)>
            on_get_instance_callback) = 0;

   private:
    static void CreateAsyncWithKeyExchange(
        const FastPairDevice& device,
        absl::AnyInvocable<void(std::unique_ptr<FastPairDataEncryptor>)>
            on_get_instance_callback);

    static void CreateAsyncWithAccountKey(
        const FastPairDevice& device,
        absl::AnyInvocable<void(std::unique_ptr<FastPairDataEncryptor>)>
            on_get_instance_callback);
  };

  std::array<uint8_t, kAesBlockByteSize> EncryptBytes(
      const std::array<uint8_t, kAesBlockByteSize>& bytes_to_encrypt)
      const override;

  std::optional<std::array<uint8_t, kPublicKeyByteSize>> GetPublicKey()
      const override;

  void ParseDecryptResponse(
      const std::vector<uint8_t>& encrypted_response_bytes,
      absl::AnyInvocable<void(const std::optional<DecryptedResponse>)> callback)
      override;

  void ParseDecryptPasskey(
      const std::vector<uint8_t>& encrypted_passkey_bytes,
      absl::AnyInvocable<void(const std::optional<DecryptedPasskey>)> callback)
      override;

  explicit FastPairDataEncryptorImpl(const KeyPair& key_pair);
  explicit FastPairDataEncryptorImpl(
      const std::array<uint8_t, kSharedSecretKeyByteSize>& secret_key);
  ~FastPairDataEncryptorImpl() override;

 private:
  const std::array<uint8_t, kSharedSecretKeyByteSize> shared_secret_key_;

  // The public key is only required during initial pairing and optional during
  // communication with paired devices.
  const std::optional<std::array<uint8_t, kPublicKeyByteSize>> public_key_ =
      std::nullopt;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_HANDSHAKE_FAST_PAIR_DATA_ENCRYPTOR_IMPL_H_
