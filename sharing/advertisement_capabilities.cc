// Copyright 2025 Google LLC
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

#include "sharing/advertisement_capabilities.h"

#include <cstdint>
#include <vector>

#include "absl/types/span.h"
#include "sharing/internal/public/logging.h"

namespace nearby::sharing {

constexpr uint8_t kFileSyncMask = 0b00000001;

AdvertisementCapabilities AdvertisementCapabilities::Parse(
    absl::Span<const uint8_t> data) {
  AdvertisementCapabilities capabilities{};
  for (const uint8_t byte : data) {
    switch (byte) {
      case static_cast<uint8_t>(Capability::kFileSync):
        capabilities.Add(Capability::kFileSync);
        break;
      default:
        continue;
    }
  }
  return capabilities;
}

std::vector<uint8_t> AdvertisementCapabilities::ToBytes() const {
  std::vector<uint8_t> bytes;
  for (const Capability capability : capabilities_) {
    switch (capability) {
      case Capability::kFileSync:
        if (bytes.empty()) {
          bytes.resize(1);
        }
        bytes[0] |= kFileSyncMask;
        break;
      default:
        LOG(DFATAL) << "Unhandled capability: "
                    << static_cast<uint8_t>(capability);
        break;
    }
  }
  return bytes;
}

}  // namespace nearby::sharing
