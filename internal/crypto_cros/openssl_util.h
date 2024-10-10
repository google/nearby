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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_CRYPTO_OPENSSL_UTIL_H_
#define THIRD_PARTY_NEARBY_INTERNAL_CRYPTO_OPENSSL_UTIL_H_

#include <stddef.h>
#include <string.h>

#include "internal/crypto_cros/crypto_export.h"

namespace nearby::crypto {

// Provides a buffer of at least MIN_SIZE bytes, for use when calling OpenSSL's
// SHA256, HMAC, etc functions, adapting the buffer sizing rules to meet those
// of our base wrapper APIs.
// This allows the library to write directly to the caller's buffer if it is of
// sufficient size, but if not it will write to temporary |min_sized_buffer_|
// of required size and then its content is automatically copied out on
// destruction, with truncation as appropriate.
template <int MIN_SIZE>
class ScopedOpenSSLSafeSizeBuffer {
 public:
  ScopedOpenSSLSafeSizeBuffer(unsigned char* output, size_t output_len)
      : output_(output), output_len_(output_len) {}

  ScopedOpenSSLSafeSizeBuffer(const ScopedOpenSSLSafeSizeBuffer&) = delete;
  ScopedOpenSSLSafeSizeBuffer& operator=(const ScopedOpenSSLSafeSizeBuffer&) =
      delete;

  ~ScopedOpenSSLSafeSizeBuffer() {
    if (output_len_ < MIN_SIZE) {
      // Copy the temporary buffer out, truncating as needed.
      memcpy(output_, min_sized_buffer_, output_len_);
    }
    // else... any writing already happened directly into |output_|.
  }

  unsigned char* safe_buffer() {
    return output_len_ < MIN_SIZE ? min_sized_buffer_ : output_;
  }

 private:
  // Pointer to the caller's data area and its associated size, where data
  // written via safe_buffer() will [eventually] end up.
  unsigned char* output_;
  size_t output_len_;

  // Temporary buffer written into in the case where the caller's
  // buffer is not of sufficient size.
  unsigned char min_sized_buffer_[MIN_SIZE];
};

// Initialize OpenSSL if it isn't already initialized. This must be called
// before any other OpenSSL functions though it is safe and cheap to call this
// multiple times.
// This function is thread-safe, and OpenSSL will only ever be initialized once.
// OpenSSL will be properly shut down on program exit.
CRYPTO_EXPORT void EnsureOpenSSLInit();

// Drains the OpenSSL ERR_get_error stack. On a debug build the error codes
// are send to VLOG(1), on a release build they are disregarded. In most
// cases you should pass absl::SourceLocation::current() as the |location|.
// TODO(b/239260254): Currently source_location.h is not available in OSS.
// We can not use std::SourceLocation as it is C++20. Once we move to C++20 or
// absl::SourceLocation is in OSS, add back "location".
// See google3/third_party/castlite/agent/crypto/openssl_util.h for reference.
CRYPTO_EXPORT void ClearOpenSSLERRStack();

// Place an instance of this class on the call stack to automatically clear
// the OpenSSL error stack on function exit.
class OpenSSLErrStackTracer {
 public:
  // Pass absl::SourceLocation::current() as |location|, to help track the
  // source of OpenSSL error messages. Note any diagnostic emitted will be
  // tagged with the location of the constructor call as it's not possible to
  // trace a destructor's callsite.
  // TODO(b/239260254): Currently source_location.h is not available in OSS.
  // We can not use std::SourceLocation as it is C++20. Once we move to C++20 or
  // absl::SourceLocation is in OSS, add back "location".
  // See google3/third_party/castlite/agent/crypto/openssl_util.h for reference.
  OpenSSLErrStackTracer() { EnsureOpenSSLInit(); }

  OpenSSLErrStackTracer(const OpenSSLErrStackTracer&) = delete;
  OpenSSLErrStackTracer& operator=(const OpenSSLErrStackTracer&) = delete;

  ~OpenSSLErrStackTracer() { ClearOpenSSLERRStack(); }
};

}  // namespace nearby::crypto

#endif  // THIRD_PARTY_NEARBY_INTERNAL_CRYPTO_OPENSSL_UTIL_H_
