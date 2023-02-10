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

#include "fastpair/handshake/fast_pair_data_encryptor_impl.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "fastpair/common/constant.h"
#include "fastpair/common/protocol.h"
#include "fastpair/crypto/decrypted_passkey.h"
#include "fastpair/crypto/decrypted_response.h"
#include "fastpair/crypto/fast_pair_encryption.h"
#include "fastpair/crypto/fast_pair_key_pair.h"
#include "fastpair/dataparser/fast_pair_data_parser.h"
#include "fastpair/handshake/fast_pair_data_encryptor.h"
#include "fastpair/repository/device_metadata.h"
#include "fastpair/server_access/fast_pair_repository.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace fastpair {

namespace {
FastPairDataEncryptorImpl::Factory* g_test_factory_ = nullptr;

bool ValidateInputSize(const std::vector<uint8_t>& encrypted_bytes) {
  if (encrypted_bytes.size() != kAesBlockByteSize) {
    NEARBY_LOGS(VERBOSE) << __func__ << ": Encrypted bytes should have size = "
                         << kAesBlockByteSize
                         << ", actual =  " << encrypted_bytes.size();
    return false;
  }
  return true;
}

}  // namespace

// FastPairDataEncryptorImpl::Factory
void FastPairDataEncryptorImpl::Factory::SetFactoryForTesting(
    Factory* g_test_factory) {
  g_test_factory_ = g_test_factory;
}

FastPairDataEncryptorImpl::Factory::~Factory() = default;

void FastPairDataEncryptorImpl::Factory::CreateAsync(
    const FastPairDevice& device,
    absl::AnyInvocable<void(std::unique_ptr<FastPairDataEncryptor>)>
        on_get_instance_callback) {
  if (g_test_factory_) {
    g_test_factory_->CreateInstance(device,
                                    std::move(on_get_instance_callback));
    return;
  }

  if (device.protocol == Protocol::kFastPairInitialPairing) {
    CreateAsyncWithKeyExchange(device, std::move(on_get_instance_callback));
  }
}

void FastPairDataEncryptorImpl::Factory::CreateAsyncWithKeyExchange(
    const FastPairDevice& device,
    absl::AnyInvocable<void(std::unique_ptr<FastPairDataEncryptor>)>
        on_get_instance_callback) {
  // We first have to get the metadata in order to get the public key to use
  // to generate the new secret key pair.
  NEARBY_LOGS(INFO) << __func__ << ": Attempting to get device metadata.";
  FastPairRepository::Get()->GetDeviceMetadata(
      device.model_id,
      [&device, &on_get_instance_callback](DeviceMetadata& metadata) {
        FastPairDataEncryptorImpl::Factory::DeviceMetadataRetrieved(
            device, std::move(on_get_instance_callback), metadata);
      });
}

void FastPairDataEncryptorImpl::Factory::DeviceMetadataRetrieved(
    const FastPairDevice& device,
    absl::AnyInvocable<void(std::unique_ptr<FastPairDataEncryptor>)>
        on_get_instance_callback,
    DeviceMetadata& device_metadata) {
  DCHECK(&device_metadata);
  std::optional<KeyPair> key_pair =
      FastPairEncryption::GenerateKeysWithEcdhKeyAgreement(
          device_metadata.GetDetails().anti_spoofing_key_pair().public_key());
  if (!key_pair.has_value()) {
    NEARBY_LOGS(INFO) << "Fail to generate key pair";
    on_get_instance_callback(nullptr);
    return;
  }

  std::unique_ptr<FastPairDataEncryptor> data_encryptor =
      std::make_unique<FastPairDataEncryptorImpl>(key_pair.value());
  on_get_instance_callback(std::move(data_encryptor));
}

// FastPairDataEncryptorImpl
FastPairDataEncryptorImpl::FastPairDataEncryptorImpl(const KeyPair& key_pair)
    : shared_secret_key_(key_pair.shared_secret_key),
      public_key_(key_pair.public_key) {}

FastPairDataEncryptorImpl::FastPairDataEncryptorImpl(
    const std::array<uint8_t, kSharedSecretKeyByteSize>& shared_secret_key)
    : shared_secret_key_(shared_secret_key) {}

FastPairDataEncryptorImpl::~FastPairDataEncryptorImpl() = default;

std::array<uint8_t, kAesBlockByteSize> FastPairDataEncryptorImpl::EncryptBytes(
    const std::array<uint8_t, kAesBlockByteSize>& bytes_to_encrypt) {
  return FastPairEncryption::EncryptBytes(shared_secret_key_, bytes_to_encrypt);
}

std::optional<std::array<uint8_t, kPublicKeyByteSize>>&
FastPairDataEncryptorImpl::GetPublicKey() {
  return public_key_;
}

void FastPairDataEncryptorImpl::ParseDecryptResponse(
    const std::vector<uint8_t>& encrypted_response_bytes,
    absl::AnyInvocable<void(const std::optional<DecryptedResponse>)> callback) {
  if (!ValidateInputSize(encrypted_response_bytes)) {
    callback(std::nullopt);
    return;
  }

  FastPairDataParser::ParseDecryptedResponse(
      std::vector<uint8_t>(shared_secret_key_.begin(),
                           shared_secret_key_.end()),
      encrypted_response_bytes,
      {
          .on_decrypted_cb = std::move(callback),
      });
}

void FastPairDataEncryptorImpl::ParseDecryptPasskey(
    const std::vector<uint8_t>& encrypted_passkey_bytes,
    absl::AnyInvocable<void(const std::optional<DecryptedPasskey>)> callback) {
  if (!ValidateInputSize(encrypted_passkey_bytes)) {
    callback(std::nullopt);
    return;
  }
  FastPairDataParser::ParseDecryptedPasskey(
      std::vector<uint8_t>(shared_secret_key_.begin(),
                           shared_secret_key_.end()),
      encrypted_passkey_bytes,
      {
          .on_decrypted_cb = std::move(callback),
      });
}
}  // namespace fastpair
}  // namespace nearby
