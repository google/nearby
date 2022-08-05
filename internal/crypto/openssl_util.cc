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

#include "internal/crypto/openssl_util.h"

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "absl/strings/string_view.h"
#include "internal/platform/logging.h"
#include <openssl/crypto.h>
#include <openssl/err.h>

namespace crypto {

namespace {

// Callback routine for OpenSSL to print error messages. |str| is a
// NULL-terminated string of length |len| containing diagnostic information
// such as the library, function and reason for the error, the file and line
// where the error originated, plus potentially any context-specific
// information about the error. |context| contains a pointer to user-supplied
// data, which is currently unused.
// If this callback returns a value <= 0, OpenSSL will stop processing the
// error queue and return, otherwise it will continue calling this function
// until all errors have been removed from the queue.
int OpenSSLErrorCallback(const char* str, size_t len, void* context) {
  NEARBY_LOGS(VERBOSE) << "\t" << absl::string_view(str, len);
  return 1;
}

}  // namespace

void EnsureOpenSSLInit() {
  // CRYPTO_library_init may be safely called concurrently.
  CRYPTO_library_init();
}

void ClearOpenSSLERRStack() {
  if (NEARBY_LOG_IS_ON(VERBOSE)) {
    uint32_t error_num = ERR_peek_error();
    if (error_num == 0) return;
    ERR_print_errors_cb(&OpenSSLErrorCallback, nullptr);
  } else {
    ERR_clear_error();
  }
}

}  // namespace crypto
