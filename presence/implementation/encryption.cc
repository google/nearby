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

#include "presence/implementation/encryption.h"

#include <algorithm>
#include <cstring>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "internal/platform/logging.h"
#include <openssl/cipher.h>  // NOLINT
#include <openssl/crypto.h>
#include <openssl/evp.h>  // NOLINT
#include "third_party/tink/cc/subtle/hkdf.h"
#include "third_party/tink/cc/subtle/random.h"

namespace nearby {
namespace presence {

using ::crypto::tink::subtle::HashType;
using ::crypto::tink::subtle::Hkdf;
using ::crypto::tink::subtle::Random;
constexpr int kAuthenticityKeyByteSize = 16;
constexpr int kMetadataKeyMaxSize = 16;
constexpr int kAesCtrIvSize = 16;
constexpr int kSaltSize = 2;

std::string Encryption::CustomizeBytesSize(absl::string_view bytes,
                                           size_t len) {
  auto result =
      Hkdf::ComputeHkdf(HashType::SHA256, /*ikm=*/bytes,
                        /*salt=*/std::string(kAuthenticityKeyByteSize, 0),
                        /*info=*/"", /*out_len=*/len);
  return result.value();
}

std::string Encryption::GenerateRandomByteArray(size_t len) {
  return Random::GetRandomBytes(len);
}

absl::StatusOr<std::string> Encryption::RunMetadataEncryption(
    absl::string_view metadata, absl::string_view key, absl::string_view salt,
    bool encrypt) {
  if (metadata.size() > kMetadataKeyMaxSize) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Metadata key length %d greater than %d",
                        metadata.size(), kMetadataKeyMaxSize));
  }
  if (key.size() != kAuthenticityKeyByteSize) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Invalid authenticity key length %d. Expected %d",
                        key.size(), kAuthenticityKeyByteSize));
  }
  if (salt.size() != kSaltSize) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Invalid salt length %d, Expected %d.", salt.size(), kSaltSize));
  }
  auto output = std::string(metadata.size(), 0);
  int output_size;
  std::string iv = CustomizeBytesSize(salt, kAesCtrIvSize);

  // AES-CTR is used without authentication because it's used as a PRF.
  auto ctx =
      std::unique_ptr<EVP_CIPHER_CTX, std::function<void(EVP_CIPHER_CTX*)>>(
          EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
  if (1 != EVP_CipherInit_ex(ctx.get(), EVP_aes_128_ctr(), nullptr,
                             reinterpret_cast<const uint8_t*>(key.data()),
                             reinterpret_cast<const uint8_t*>(iv.data()),
                             encrypt ? 1 : 0)) {
    return absl::InvalidArgumentError("Failed to initialize AES encryption.");
  }

  int input_size = metadata.size();
  if (1 != EVP_CipherUpdate(
               ctx.get(), reinterpret_cast<uint8_t*>(output.data()),
               &output_size, reinterpret_cast<const uint8_t*>(metadata.data()),
               input_size)) {
    return absl::InvalidArgumentError("AES error in EVP_CipherUpdate");
  }
  int tmp_size = 0;
  if (1 != EVP_EncryptFinal_ex(
               ctx.get(),
               reinterpret_cast<uint8_t*>(output.data() + output_size),
               &tmp_size)) {
    return absl::InvalidArgumentError("AES errorin EVP_EncryptFinal_ex");
  }
  output_size += tmp_size;
  if (output_size != input_size) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Invalid output size %d. Expected %d", output_size, input_size));
  }
  return output;
}

}  // namespace presence
}  // namespace nearby
