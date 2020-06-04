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

#ifndef CORE_INTERNAL_MEDIUMS_UUID_H_
#define CORE_INTERNAL_MEDIUMS_UUID_H_

#include <cstdint>

#include "platform/port/string.h"

namespace location {
namespace nearby {
namespace connections {

// A type 3 name-based
// (https://en.wikipedia.org/wiki/Universally_unique_identifier#Versions_3_and_5_(namespace_name-based))
// UUID.
//
// https://developer.android.com/reference/java/util/UUID.html
template <typename Platform>
class UUID {
 public:
  explicit UUID(const std::string& data);
  UUID(std::int64_t most_sig_bits, std::int64_t least_sig_bits);
  ~UUID();

  // Returns the canonical textual representation
  // (https://en.wikipedia.org/wiki/Universally_unique_identifier#Format) of the
  // UUID.
  std::string str();

 private:
  std::string data_;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#include "core/internal/mediums/uuid.cc"

#endif  // CORE_INTERNAL_MEDIUMS_UUID_H_
