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

#include "internal/crypto/random.h"

#include <stddef.h>

#include <string>

#include <openssl/rand.h>

namespace crypto {

void RandBytes(void *bytes, size_t length) {
  RAND_bytes(reinterpret_cast<uint8_t *>(bytes), length);
}

void RandBytes(absl::Span<uint8_t> bytes) {
  RandBytes(bytes.data(), bytes.size());
}

std::string RandBytes(size_t length) {
  std::string result(length, 0);
  RandBytes(const_cast<std::string::value_type *>(result.data()),
            result.size());
  return result;
}

}  // namespace crypto
