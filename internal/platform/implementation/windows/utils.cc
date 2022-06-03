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

#include "internal/platform/implementation/windows/utils.h"

// Windows headers
#include <inaddr.h>
#include <stdlib.h>
#include <strsafe.h>
#include <winsock.h>

// Standard C/C++ headers
#include <codecvt>
#include <cstdint>
#include <exception>
#include <string>

// Third party headers
#include "absl/strings/ascii.h"
#include "absl/strings/str_format.h"

// Nearby connections headers
#include "absl/strings/string_view.h"
#include "internal/platform/bluetooth_utils.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/implementation/crypto.h"

namespace location {
namespace nearby {
namespace windows {

std::string uint64_to_mac_address_string(uint64_t bluetoothAddress) {
  std::string buffer = absl::StrFormat(
      "%2llx:%2llx:%2llx:%2llx:%2llx:%2llx", bluetoothAddress >> 40,
      (bluetoothAddress >> 32) & 0xff, (bluetoothAddress >> 24) & 0xff,
      (bluetoothAddress >> 16) & 0xff, (bluetoothAddress >> 8) & 0xff,
      bluetoothAddress & 0xff);

  return absl::AsciiStrToUpper(buffer);
}

uint64_t mac_address_string_to_uint64(absl::string_view mac_address) {
  ByteArray mac_address_array = BluetoothUtils::FromString(mac_address);
  uint64_t mac_address_uint64 = 0;
  for (int i = 0; i < mac_address_array.size(); i++) {
    mac_address_uint64 <<= 8;
    mac_address_uint64 |= static_cast<uint8_t>(
        static_cast<unsigned char>(*(mac_address_array.data() + i)));
  }
  return mac_address_uint64;
}

std::string ipaddr_4bytes_to_dotdecimal_string(
    absl::string_view ipaddr_4bytes) {
  in_addr address;
  address.S_un.S_un_b.s_b1 = ipaddr_4bytes[0];
  address.S_un.S_un_b.s_b2 = ipaddr_4bytes[1];
  address.S_un.S_un_b.s_b3 = ipaddr_4bytes[2];
  address.S_un.S_un_b.s_b4 = ipaddr_4bytes[3];
  char* ipv4_address = inet_ntoa(address);
  if (ipv4_address == nullptr) {
    return {};
  }

  return std::string(ipv4_address);
}

std::string ipaddr_dotdecimal_to_4bytes_string(std::string ipv4_s) {
  in_addr address;
  address.S_un.S_addr = inet_addr(ipv4_s.c_str());
  char ipv4_b[5];
  ipv4_b[0] = address.S_un.S_un_b.s_b1;
  ipv4_b[1] = address.S_un.S_un_b.s_b2;
  ipv4_b[2] = address.S_un.S_un_b.s_b3;
  ipv4_b[3] = address.S_un.S_un_b.s_b4;
  ipv4_b[4] = 0;

  return std::string(ipv4_b, 4);
}

std::wstring string_to_wstring(std::string str) {
  std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
  return converter.from_bytes(str);
}

std::string wstring_to_string(std::wstring wstr) {
  std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
  return converter.to_bytes(wstr);
}

ByteArray Sha256(absl::string_view input, size_t size) {
  ByteArray hash = location::nearby::Crypto::Sha256(input);
  return ByteArray{hash.data(), size};
}

uint16 InspectableReader::ReadUint16(IInspectable inspectable) {
  auto property_value =
      inspectable.try_as<winrt::Windows::Foundation::IPropertyValue>();
  if (property_value == nullptr) {
    throw std::invalid_argument("no property value interface.");
  }
  if (property_value.Type() !=
      winrt::Windows::Foundation::PropertyType::UInt16) {
    throw std::invalid_argument("not uin16 data type.");
  }

  return property_value.GetUInt16();
}

uint32 InspectableReader::ReadUint32(IInspectable inspectable) {
  auto property_value =
      inspectable.try_as<winrt::Windows::Foundation::IPropertyValue>();
  if (property_value == nullptr) {
    throw std::invalid_argument("no property value interface.");
  }
  if (property_value.Type() !=
      winrt::Windows::Foundation::PropertyType::UInt32) {
    throw std::invalid_argument("not uin32 data type.");
  }

  return property_value.GetUInt32();
}

std::string InspectableReader::ReadString(IInspectable inspectable) {
  auto property_value =
      inspectable.try_as<winrt::Windows::Foundation::IPropertyValue>();
  if (property_value == nullptr) {
    throw std::invalid_argument("no property value interface.");
  }
  if (property_value.Type() !=
      winrt::Windows::Foundation::PropertyType::String) {
    throw std::invalid_argument("not string data type.");
  }

  return wstring_to_string(property_value.GetString().c_str());
}

std::vector<std::string> InspectableReader::ReadStringArray(
    IInspectable inspectable) {
  std::vector<std::string> result;
  auto property_value =
      inspectable.try_as<winrt::Windows::Foundation::IPropertyValue>();
  if (property_value == nullptr) {
    throw std::invalid_argument("no property value interface.");
  }
  if (property_value.Type() !=
      winrt::Windows::Foundation::PropertyType::StringArray) {
    throw std::invalid_argument("not string array data type.");
  }

  winrt::com_array<winrt::hstring> strings;
  property_value.GetStringArray(strings);

  for (winrt::hstring str : strings) {
    result.push_back(winrt::to_string(str));
  }
  return result;
}

}  // namespace windows
}  // namespace nearby
}  // namespace location
