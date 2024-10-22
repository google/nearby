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

#ifndef CORE_INTERNAL_MEDIUMS_UTILS_H_
#define CORE_INTERNAL_MEDIUMS_UTILS_H_

#include <cstddef>
#include <string>

#include "connections/implementation/proto/offline_wire_formats.pb.h"
#include "internal/platform/byte_array.h"

namespace nearby {
namespace connections {

class Utils {
 public:
  static ByteArray GenerateRandomBytes(size_t length);
  static ByteArray Sha256Hash(const ByteArray& source, size_t length);
  static ByteArray Sha256Hash(const std::string& source, size_t length);
  static location::nearby::connections::LocationHint BuildLocationHint(
      const std::string& location);
  static std::string GenerateSalt();
  static std::string GenerateSalt(size_t length);
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_MEDIUMS_UTILS_H_
