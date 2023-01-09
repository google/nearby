// Copyright 2021 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_BASE_BLUETOOTH_ADDRESS_H_
#define THIRD_PARTY_NEARBY_INTERNAL_BASE_BLUETOOTH_ADDRESS_H_

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <string>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"

namespace nearby {
namespace device {

// Parses a Bluetooth address to an output buffer. The output buffer must be
// exactly 6 bytes in size. The address can be formatted in one of three ways:
//
//   1A:2B:3C:4D:5E:6F
//   1A-2B-3C-4D-5E-6F
//   1A2B3C4D5E6F
bool ParseBluetoothAddress(absl::string_view input, absl::Span<uint8_t> output);

// Converts a uint64_t Bluetooth address to string.
std::string ConvertBluetoothAddressUIntToString(uint64_t address);

// Returns |address| in the canonical format: XX:XX:XX:XX:XX:XX, where each 'X'
// is a hex digit.  If the input |address| is invalid, returns an empty string.
std::string CanonicalizeBluetoothAddress(absl::string_view address);
std::string CanonicalizeBluetoothAddress(
    const std::array<uint8_t, 6>& address_bytes);
std::string CanonicalizeBluetoothAddress(uint64_t address);

}  // namespace device
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_BASE_BLUETOOTH_ADDRESS_H_
