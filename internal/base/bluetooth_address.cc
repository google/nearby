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

#include "internal/base/bluetooth_address.h"

#include <algorithm>
#include <array>
#include <string>

#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"

namespace nearby {
namespace device {
namespace {

template <int BASE, typename CHAR>
// Note that some of the methods return absl::optional instead
// of std::optional, because iOS platform is still in C++14.
absl::optional<uint8_t> CharToDigit(CHAR c) {
  static_assert(1 <= BASE && BASE <= 36, "BASE needs to be in [1, 36]");
  if (c >= '0' && c < '0' + std::min(BASE, 10)) return c - '0';

  if (c >= 'a' && c < 'a' + BASE - 10) return c - 'a' + 10;

  if (c >= 'A' && c < 'A' + BASE - 10) return c - 'A' + 10;

  return absl::nullopt;
}

template <typename OutIter>
static bool HexStringToByteContainer(absl::string_view input, OutIter output) {
  size_t count = input.size();
  if (count == 0 || (count % 2) != 0) return false;
  for (uintptr_t i = 0; i < count / 2; ++i) {
    // most significant 4 bits
    absl::optional<uint8_t> msb = CharToDigit<16>(input[i * 2]);
    // least significant 4 bits
    absl::optional<uint8_t> lsb = CharToDigit<16>(input[i * 2 + 1]);
    if (!msb.has_value() || !lsb.has_value()) {
      return false;
    }
    *(output++) = (*msb << 4) | *lsb;
  }
  return true;
}

bool HexStringToSpan(absl::string_view input, absl::Span<uint8_t> output) {
  if (input.size() / 2 != output.size()) return false;

  return HexStringToByteContainer(input, output.begin());
}

}  // namespace

bool ParseBluetoothAddress(absl::string_view input,
                           absl::Span<uint8_t> output) {
  if (output.size() != 6) return false;

  // Try parsing addresses that lack separators, like "1A2B3C4D5E6F".
  if (input.size() == 12) return HexStringToSpan(input, output);

  // Try parsing MAC address with separators like: "00:11:22:33:44:55" or
  // "00-11-22-33-44-55". Separator can be either '-' or ':', but must use the
  // same style throughout.
  if (input.size() == 17) {
    const char separator = input[2];
    if (separator != '-' && separator != ':') return false;
    return (input[2] == separator) && (input[5] == separator) &&
           (input[8] == separator) && (input[11] == separator) &&
           (input[14] == separator) &&
           HexStringToSpan(input.substr(0, 2), output.subspan(0, 1)) &&
           HexStringToSpan(input.substr(3, 2), output.subspan(1, 1)) &&
           HexStringToSpan(input.substr(6, 2), output.subspan(2, 1)) &&
           HexStringToSpan(input.substr(9, 2), output.subspan(3, 1)) &&
           HexStringToSpan(input.substr(12, 2), output.subspan(4, 1)) &&
           HexStringToSpan(input.substr(15, 2), output.subspan(5, 1));
  }

  return false;
}

std::string ConvertBluetoothAddressUIntToString(uint64_t address) {
  std::string mac_address = absl::StrFormat(
      "%02llX:%02llX:%02llX:%02llX:%02llX:%02llX", address >> 40,
      (address >> 32) & 0xff, (address >> 24) & 0xff, (address >> 16) & 0xff,
      (address >> 8) & 0xff, address & 0xff);
  return CanonicalizeBluetoothAddress(mac_address);
}

std::string CanonicalizeBluetoothAddress(absl::string_view address) {
  std::array<uint8_t, 6> bytes;

  if (!ParseBluetoothAddress(address, absl::MakeSpan(bytes.data(), 6)))
    return std::string();

  return CanonicalizeBluetoothAddress(bytes);
}

std::string CanonicalizeBluetoothAddress(
    const std::array<uint8_t, 6>& address_bytes) {
  return absl::StrFormat("%02X:%02X:%02X:%02X:%02X:%02X", address_bytes[0],
                         address_bytes[1], address_bytes[2], address_bytes[3],
                         address_bytes[4], address_bytes[5]);
}

std::string CanonicalizeBluetoothAddress(uint64_t address) {
  return ConvertBluetoothAddressUIntToString(address);
}

}  // namespace device
}  // namespace nearby
