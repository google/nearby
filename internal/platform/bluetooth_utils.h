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

#ifndef PLATFORM_BASE_BLUETOOTH_UTILS_H_
#define PLATFORM_BASE_BLUETOOTH_UTILS_H_

#include <cstdint>
#include <string>

#include "absl/strings/string_view.h"
#include "internal/platform/byte_array.h"

namespace nearby {

class BluetoothUtils {
 public:
  static constexpr int kBluetoothMacAddressLength = 6;

  // Converts a Bluetooth MAC address from byte array to String format. Returns
  // empty if input byte array is not of correct format.
  // e.g. {-84, 55, 67, -68, -87, 40} -> "AC:37:43:BC:A9:28".
  static std::string ToString(const ByteArray& bluetooth_mac_address);

  // Converts a Bluetooth MAC address from String format to byte array. Returns
  // empty if input string is not of correct format.
  // e.g. "AC:37:43:BC:A9:28" -> {-84, 55, 67, -68, -87, 40}.
  static ByteArray FromString(absl::string_view bluetooth_mac_address);

  // Converts a MAC address from binary to canonical format.
  // Example: 0xF1F2F3F4F5F6 -> "F1:F2:F3:F4:F5:F6"
  static std::string FromNumber(std::uint64_t address);

  // Converts a MAC address from canonical format to binary
  // Example: "F1:F2:F3:F4:F5:F6" ->0xF1F2F3F4F5F6
  static std::uint64_t ToNumber(absl::string_view address);

  // Checks if a Bluetooth MAC address is zero for every byte.
  static bool IsBluetoothMacAddressUnset(
      const ByteArray& bluetooth_mac_address);
};

}  // namespace nearby

#endif  // PLATFORM_BASE_BLUETOOTH_UTILS_H_
