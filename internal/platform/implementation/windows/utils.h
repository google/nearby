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

#ifndef PLATFORM_IMPL_WINDOWS_UTILS_H_
#define PLATFORM_IMPL_WINDOWS_UTILS_H_

#include <Windows.h>
#include <stdio.h>

#include <string>

#include "absl/strings/string_view.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Foundation.h"
#include "internal/platform/implementation/windows/generated/winrt/base.h"

namespace location {
namespace nearby {
namespace windows {

using winrt::Windows::Foundation::IInspectable;

std::string uint64_to_mac_address_string(uint64_t bluetoothAddress);

// Helpers to windows platform
std::wstring string_to_wstring(std::string str);
std::string wstring_to_string(std::wstring wstr);
ByteArray Sha256(absl::string_view input, size_t size);

namespace Constants {
// The Id of the Service Name SDP attribute
const uint16_t SdpServiceNameAttributeId = 0x100;

// The SDP Type of the Service Name SDP attribute.
// The first byte in the SDP Attribute encodes the SDP Attribute Type as
// follows:
//    -  the Attribute Type size in the least significant 3 bits,
//    -  the SDP Attribute Type value in the most significant 5 bits.
const char SdpServiceNameAttributeType = (4 << 3) | 5;
}  // namespace Constants

class InspectableReader {
 public:
  static uint16 ReadUint16(IInspectable inspectable);
  static uint32 ReadUint32(IInspectable inspectable);
  static std::string ReadString(IInspectable inspectable);
  static std::vector<std::string> ReadStringArray(IInspectable inspectable);
};

}  // namespace windows
}  // namespace nearby
}  // namespace location

#endif  //  PLATFORM_IMPL_WINDOWS_UTILS_H_
