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

#include <systemd/sd-id128.h>

#include "internal/platform/implementation/linux/utils.h"

namespace nearby {
namespace linux {
std::optional<Uuid> UuidFromString(const std::string &uuid_str) {
  sd_id128_t uuid;
  if (auto ret = sd_id128_from_string(uuid_str.c_str(), &uuid); ret < 0)
    return std::nullopt;

  const int ONE = 1;
  if (*(reinterpret_cast<const char *>(&ONE)) ==
      1) {  // On a little endian platform
    uint64_t msb = static_cast<uint64_t>(uuid.bytes[7]) +
                   (static_cast<uint64_t>(uuid.bytes[6]) << 8) +
                   (static_cast<uint64_t>(uuid.bytes[5]) << 16) +
                   (static_cast<uint64_t>(uuid.bytes[4]) << 24) +
                   (static_cast<uint64_t>(uuid.bytes[3]) << 32) +
                   (static_cast<uint64_t>(uuid.bytes[2]) << 40) +
                   (static_cast<uint64_t>(uuid.bytes[1]) << 48) +
                   (static_cast<uint64_t>(uuid.bytes[0]) << 56);
    uint64_t lsb = static_cast<uint64_t>(uuid.bytes[15]) +
                   (static_cast<uint64_t>(uuid.bytes[14]) << 8) +
                   (static_cast<uint64_t>(uuid.bytes[13]) << 16) +
                   (static_cast<uint64_t>(uuid.bytes[12]) << 24) +
                   (static_cast<uint64_t>(uuid.bytes[11]) << 32) +
                   (static_cast<uint64_t>(uuid.bytes[10]) << 40) +
                   (static_cast<uint64_t>(uuid.bytes[9]) << 48) +
                   (static_cast<uint64_t>(uuid.bytes[8]) << 56);
    return Uuid(msb, lsb);
  }

  return Uuid(uuid.qwords[0], uuid.qwords[1]);
}
}  // namespace linux
}  // namespace nearby
