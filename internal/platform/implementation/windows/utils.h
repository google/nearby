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

#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/uuid.h"
#include "winrt/Windows.Foundation.h"
#include "winrt/base.h"

namespace nearby {
namespace windows {

using winrt::Windows::Foundation::IInspectable;

std::string uint64_to_mac_address_string(uint64_t bluetoothAddress);
uint64_t mac_address_string_to_uint64(absl::string_view mac_address);

std::string ipaddr_4bytes_to_dotdecimal_string(absl::string_view ipaddr_4bytes);
std::string ipaddr_dotdecimal_to_4bytes_string(std::string ipv4_s);

// Helpers to windows platform
ByteArray Sha256(absl::string_view input, size_t size);

// Reads the IPv4 addresses
std::vector<std::string> GetIpv4Addresses();
std::vector<std::string> Get4BytesIpv4Addresses();

// Help methods to convert between Uuid and winrt::guid
Uuid winrt_guid_to_nearby_uuid(const ::winrt::guid& guid);
winrt::guid nearby_uuid_to_winrt_guid(Uuid uuid);

// Check whether Uuid and guid is the same value.
bool is_nearby_uuid_equal_to_winrt_guid(const Uuid& uuid,
                                     const ::winrt::guid& guid);

namespace Constants {
// The Id of the Service Name SDP attribute
const uint16_t SdpServiceNameAttributeId = 0x100;

// The SDP Type of the Service Name SDP attribute.
// The first byte in the SDP Attribute encodes the SDP Attribute Type as
// follows:
//    -  the Attribute Type size in the least significant 3 bits,
//    -  the SDP Attribute Type value in the most significant 5 bits.
const char SdpServiceNameAttributeType = (4 << 3) | 5;

// Possible values for the adapter type. Refer to:
// https://learn.microsoft.com/en-us/windows/win32/api/iptypes/ns-iptypes-ip_adapter_info
const uint16_t kInterfaceTypeEthernet = 6;
const uint16_t kInterfaceTypeWifi = 71;
}  // namespace Constants

class InspectableReader {
 public:
  static bool ReadBoolean(IInspectable inspectable);
  static uint16_t ReadUint16(IInspectable inspectable);
  static uint32_t ReadUint32(IInspectable inspectable);
  static std::string ReadString(IInspectable inspectable);
  static std::vector<std::string> ReadStringArray(IInspectable inspectable);
};

}  // namespace windows
}  // namespace nearby

#endif  //  PLATFORM_IMPL_WINDOWS_UTILS_H_
