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

#include "internal/crypto_cros/secure_util.h"

#include <openssl/mem.h>

namespace nearby::crypto {

bool SecureMemEqual(const void* s1, const void* s2, size_t n) {
  return CRYPTO_memcmp(s1, s2, n) == 0;
}

}  // namespace nearby::crypto
