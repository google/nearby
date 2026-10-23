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

#ifndef THIRD_PARTY_NEARBY_SHARING_ADVERTISEMENT_CAPABILITIES_H_
#define THIRD_PARTY_NEARBY_SHARING_ADVERTISEMENT_CAPABILITIES_H_

#include <cstdint>
#include <initializer_list>
#include <vector>
#include "absl/types/span.h"

namespace nearby::sharing {

// A container class for storing capabilities to be added to QuickShare
// advertisements.
class AdvertisementCapabilities {
 public:
  enum class Capability {
    kInvalid = 0,
    kFileSync = 1,  // File sync extension support.
  };

  // Parses serialized capabilities from an advertisement.
  static AdvertisementCapabilities Parse(absl::Span<const uint8_t> data);

  AdvertisementCapabilities(std::initializer_list<Capability> capabilities)
      : capabilities_(capabilities) {}

  void Add(Capability capability) { capabilities_.push_back(capability); }

  // Returns true if there are no capabilities in this object.
  bool IsEmpty() const { return capabilities_.empty(); }

  // Serializes the capabilities into a byte array for inclusion in an
  // advertisement.
  std::vector<uint8_t> ToBytes() const;

 private:
  std::vector<Capability> capabilities_;
};

}  // namespace nearby::sharing

#endif  // THIRD_PARTY_NEARBY_SHARING_ADVERTISEMENT_CAPABILITIES_H_
