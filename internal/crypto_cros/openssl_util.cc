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

#include "internal/crypto_cros/openssl_util.h"

#include <stddef.h>
#include <stdint.h>

#include <string>

#ifdef NEARBY_CHROMIUM
#include "internal/platform/logging.h"
#elif defined(NEARBY_SWIFTPM)
#include "internal/platform/logging.h"
#else
#include "absl/log/log.h"  // nogncheck
#endif
#include "absl/strings/string_view.h"
#include <openssl/crypto.h>
#include <openssl/err.h>

namespace nearby::crypto {

void EnsureOpenSSLInit() {
  // CRYPTO_library_init may be safely called concurrently.
  CRYPTO_library_init();
}

void ClearOpenSSLERRStack() {
    ERR_clear_error();
}

}  // namespace nearby::crypto
