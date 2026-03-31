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

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "internal/platform/uuid.h"
#include "winrt/Windows.Foundation.h"
#include "winrt/base.h"

namespace nearby {
namespace windows {

// Help methods to convert between Uuid and winrt::guid
Uuid winrt_guid_to_nearby_uuid(const ::winrt::guid& guid);
winrt::guid nearby_uuid_to_winrt_guid(Uuid uuid);

// Check whether Uuid and guid is the same value.
bool is_nearby_uuid_equal_to_winrt_guid(const Uuid& uuid,
                                        const ::winrt::guid& guid);

// Returns the DNS host name of the computer or std::nullopt if it fails.
std::optional<std::wstring> GetDnsHostName();

// Returns true if the system has an Intel Wi-Fi adapter.
bool IsIntelWifiAdapter();

class InspectableReader {
 public:
  static bool ReadBoolean(winrt::Windows::Foundation::IInspectable inspectable);
  static uint16_t ReadUint16(
      winrt::Windows::Foundation::IInspectable inspectable);
  static uint32_t ReadUint32(
      winrt::Windows::Foundation::IInspectable inspectable);
  static std::string ReadString(
      winrt::Windows::Foundation::IInspectable inspectable);
  static std::vector<std::string> ReadStringArray(
      winrt::Windows::Foundation::IInspectable inspectable);
  static GUID ReadGuid(winrt::Windows::Foundation::IInspectable inspectable);
};

}  // namespace windows
}  // namespace nearby

#endif  //  PLATFORM_IMPL_WINDOWS_UTILS_H_
