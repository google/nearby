// Copyright 2022 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_DATA_ELEMENT_H_
#define THIRD_PARTY_NEARBY_PRESENCE_DATA_ELEMENT_H_

#include <stdint.h>

#include <string>

#include "absl/strings/string_view.h"
namespace nearby {
namespace presence {

/** Describes a custom Data element in NP advertisement. */
class DataElement {
 public:
  DataElement(uint16_t type, absl::string_view value)
      : type_(type), value_(value) {}

  uint16_t GetType() const { return type_; }
  absl::string_view GetValue() const { return value_; }

  static constexpr int kContextTimestamp = 1 << 12;
  // The values below match the bitmasks in Base NP Intent.
  static constexpr int kActiveUnlock = 1 << 11;
  static constexpr int kTapToTransfer = 1 << 10;
  static constexpr int kNearbyShare = 1 << 9;
  static constexpr int kFastPair = 1 << 8;
  static constexpr int kFitCast = 1 << 7;
  static constexpr int kPresenceManager = 1 << 6;

 private:
  uint16_t type_;
  std::string value_;
};
}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_DATA_ELEMENT_H_
