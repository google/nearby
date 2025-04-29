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

#include "internal/platform/mac_address.h"

#include <cstdint>
#include <string>

#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"

namespace nearby {

bool MacAddress::FromString(absl::string_view address,
                            MacAddress& mac_address) {
  if (address.length() != 17) {
    return false;
  }
  uint64_t address_int = 0;
  for (int i = 0; i < 6; ++i) {
    // Check for ":" separator.
    if (i < 5 && address[i * 3 + 2] != ':') {
      return false;
    }
    absl::string_view component;
    component = address.substr(i * 3, 2);
    uint64_t component_int = 0;
    if (!absl::SimpleHexAtoi(component, &component_int)) {
      return false;
    }
    address_int = (address_int << 8) | component_int;
  }
  mac_address.address_ = address_int;
  return true;
}


bool MacAddress::FromUint64(uint64_t address, MacAddress& mac_address) {
  if ((address & 0xffff000000000000L) != 0) {
    return false;
  }
  mac_address.address_ = address;
  return true;
}

std::string MacAddress::ToString() const {
  return absl::StrCat(absl::StrFormat("%02X", (address_ >> 40) & 0xff), ":",
                      absl::StrFormat("%02X", (address_ >> 32) & 0xff), ":",
                      absl::StrFormat("%02X", (address_ >> 24) & 0xff), ":",
                      absl::StrFormat("%02X", (address_ >> 16) & 0xff), ":",
                      absl::StrFormat("%02X", (address_ >> 8) & 0xff), ":",
                      absl::StrFormat("%02X", address_ & 0xff));
}

}  // namespace nearby
