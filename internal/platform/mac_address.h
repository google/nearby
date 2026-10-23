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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_MAC_ADDRESS_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_MAC_ADDRESS_H_

#include <cstdint>
#include <string>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"

namespace nearby {

// Representation of a 48 bit MAC address.
class MacAddress {
 public:
  // Creates an empty MAC address with all 0s.
  MacAddress() = default;

  // Creates a MAC address from a string.
  // Returns false if the string is not a valid MAC address.
  static bool FromString(absl::string_view address, MacAddress& mac_address);

  // Creates a MAC address from a 64-bit integer.
  // The upper 16 bits of the address must be 0.
  // Returns false if the integer is not a valid MAC address.
  static bool FromUint64(uint64_t address, MacAddress& mac_address);

  // Creates a MAC address from a span of bytes.
  // Returns false if the span is less than 6 bytes long.
  static bool FromBytes(absl::Span<const uint8_t> bytes,
                        MacAddress& mac_address);

  // Packs the MAC address into the lower 48 bits of a 64-bit integer.
  uint64_t address() const { return address_; }

  // Returns the MAC address in the format of "00:B0:D0:63:C2:26".
  std::string ToString() const;

  // Packs the MAC address into the given bytes.
  // Returns false if the span is less than 6 bytes long.
  bool ToBytes(absl::Span<uint8_t> bytes) const;

  // Returns true if the MAC address is set.
  bool IsSet() const { return address_ != 0; }

  // Hash function for absl containers.
  template <typename H>
  friend H AbslHashValue(H h, const MacAddress& addr) {
    return H::combine(std::move(h), addr.address_);
  }

  friend auto operator<=>(const MacAddress& lhs,
                          const MacAddress& rhs) = default;

 private:
  uint64_t address_ = 0;
};

}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_MAC_ADDRESS_H_
