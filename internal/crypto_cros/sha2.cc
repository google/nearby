// Copyright 2020 Google LLC
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

#include "internal/crypto_cros/sha2.h"

#include <stddef.h>

#include <array>
#include <memory>
#include <string>

#include "absl/strings/string_view.h"
#include "internal/crypto_cros/secure_hash.h"
#include <openssl/sha.h>

namespace crypto {

std::array<uint8_t, kSHA256Length> SHA256Hash(absl::Span<const uint8_t> input) {
  std::array<uint8_t, kSHA256Length> digest;
  ::SHA256(input.data(), input.size(), digest.data());
  return digest;
}

void SHA256HashString(absl::string_view str, void* output, size_t len) {
  std::unique_ptr<SecureHash> ctx(SecureHash::Create(SecureHash::SHA256));
  ctx->Update(str.data(), str.length());
  ctx->Finish(output, len);
}

std::string SHA256HashString(absl::string_view str) {
  std::string output(kSHA256Length, 0);
  SHA256HashString(str, &*output.begin(), output.size());
  return output;
}

}  // namespace crypto
