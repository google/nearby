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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_CRYPTO_SECURE_HASH_H_
#define THIRD_PARTY_NEARBY_INTERNAL_CRYPTO_SECURE_HASH_H_

#include <stddef.h>

#include <memory>

#include "internal/crypto/crypto_export.h"

namespace crypto {

// A wrapper to calculate secure hashes incrementally, allowing to
// be used when the full input is not known in advance. The end result will the
// same as if we have the full input in advance.
class CRYPTO_EXPORT SecureHash {
 public:
  enum Algorithm {
    SHA256,
  };

  SecureHash(const SecureHash&) = delete;
  SecureHash& operator=(const SecureHash&) = delete;

  virtual ~SecureHash() {}

  static std::unique_ptr<SecureHash> Create(Algorithm type);

  virtual void Update(const void* input, size_t len) = 0;
  virtual void Finish(void* output, size_t len) = 0;
  virtual size_t GetHashLength() const = 0;

  // Create a clone of this SecureHash. The returned clone and this both
  // represent the same hash state. But from this point on, calling
  // Update()/Finish() on either doesn't affect the state of the other.
  virtual std::unique_ptr<SecureHash> Clone() const = 0;

 protected:
  SecureHash() {}
};

}  // namespace crypto

#endif  // THIRD_PARTY_NEARBY_INTERNAL_CRYPTO_SECURE_HASH_H_
