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

#include "internal/platform/implementation/crypto.h"

#include <cstdint>
#include <string>

#include "absl/strings/string_view.h"
#include "internal/platform/byte_array.h"
#if defined NEARBY_WINDOWS_DLL
#include <openssl/boringssl/src/include/openssl/digest.h>
#else
#include <openssl/digest.h>
#endif

// Function implementations for platform/api/crypto.h.

namespace nearby {

// Initialize global crypto state.
void Crypto::Init() {}

static ByteArray Hash(absl::string_view input, const EVP_MD* algo) {
  unsigned int md_out_size = EVP_MAX_MD_SIZE;
  uint8_t digest_buffer[EVP_MAX_MD_SIZE];
  if (input.empty()) return {};

  if (!EVP_Digest(input.data(), input.size(), digest_buffer, &md_out_size, algo,
                  nullptr))
    return {};

  return ByteArray{reinterpret_cast<char*>(digest_buffer), md_out_size};
}

// Return MD5 hash of input.
ByteArray Crypto::Md5(absl::string_view input) {
  return Hash(input, EVP_md5());
}

// Return SHA256 hash of input.
ByteArray Crypto::Sha256(absl::string_view input) {
  return Hash(input, EVP_sha256());
}

}  // namespace nearby
