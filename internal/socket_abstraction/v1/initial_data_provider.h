// Copyright 2023 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_SOCKET_ABSTRACTION_V1_INITIAL_DATA_PROVIDER_H_
#define THIRD_PARTY_NEARBY_INTERNAL_SOCKET_ABSTRACTION_V1_INITIAL_DATA_PROVIDER_H_

#include "absl/random/random.h"
#include "internal/platform/byte_array.h"

namespace nearby {
namespace socket_abstraction {

class InitialDataProvider {
 public:
  virtual ~InitialDataProvider() = default;
  virtual ByteArray Provide() = 0;
};

class RandomDataProvider : public InitialDataProvider {
 public:
  explicit RandomDataProvider(int numberOfBytes)
      : number_of_bytes_(numberOfBytes) {}
  ByteArray Provide() override {
    ByteArray arr = ByteArray(number_of_bytes_);
    for (int i = 0; i < number_of_bytes_; i++) {
      arr.data()[i] = absl::Uniform<uint8_t>(prng_);
    }
    return arr;
  }

 private:
  int number_of_bytes_;
  absl::BitGen prng_;
};

class EmptyDataProvider : public InitialDataProvider {
 public:
  ByteArray Provide() override { return ByteArray(0); }
};

}  // namespace socket_abstraction
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_SOCKET_ABSTRACTION_V1_INITIAL_DATA_PROVIDER_H_
