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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_WEAVE_SOCKETS_INITIAL_DATA_PROVIDER_H_
#define THIRD_PARTY_NEARBY_INTERNAL_WEAVE_SOCKETS_INITIAL_DATA_PROVIDER_H_

#include <cmath>
#include <cstdint>
#include <string>

#include "absl/random/random.h"
#include "absl/strings/str_cat.h"

namespace nearby {
namespace weave {

class InitialDataProvider {
 public:
  virtual ~InitialDataProvider() = default;
  virtual std::string Provide() { return ""; }
};

class RandomDataProvider : public InitialDataProvider {
 public:
  explicit RandomDataProvider(int number_of_bytes)
      : number_of_bytes_(number_of_bytes) {}
  std::string Provide() override {
    uint64_t res = absl::uniform_int_distribution<uint64_t>(
        1, pow(10, static_cast<double>(number_of_bytes_)))(prng_);
    return absl::StrCat(res);
  }

 private:
  int number_of_bytes_;
  absl::BitGen prng_;
};

}  // namespace weave
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_WEAVE_SOCKETS_INITIAL_DATA_PROVIDER_H_
