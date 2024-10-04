// Copyright 2020-2024 Google LLC
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

#include <windows.h>
#include <winsock2.h>

// Standard C/C++ headers
#include <codecvt>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <locale>
#include <stdexcept>
#include <string>
#include <vector>

// Third party headers
#include "absl/strings/ascii.h"
#include "absl/strings/str_format.h"

// Nearby connections headers
#include "absl/strings/string_view.h"
#include "internal/platform/bluetooth_utils.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/implementation/crypto.h"
#include "internal/platform/implementation/windows/string_utils.h"
#include "internal/platform/logging.h"
#include "internal/platform/uuid.h"
#include "winrt/Windows.Foundation.Collections.h"
#include "winrt/Windows.Networking.Connectivity.h"
#include "winrt/base.h"

namespace nearby {
namespace windows {
namespace {

using ::winrt::Windows::Networking::HostNameType;
using ::winrt::Windows::Networking::Connectivity::NetworkAdapter;
using ::winrt::Windows::Networking::Connectivity::NetworkInformation;
using ::winrt::Windows::Networking::Connectivity::NetworkTypes;

}  // namespace

std::string uint64_to_mac_address_string(uint64_t bluetoothAddress) {
  std::string buffer = absl::StrFormat(
      "%02llx:%02llx:%02llx:%02llx:%02llx:%02llx", bluetoothAddress >> 40,
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
  if (ipv4_s.empty()) {
    return {};
  }

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

std::vector<std::string> GetIpv4Addresses() {
  std::vector<std::string> result;
  std::vector<std::string> wifi_addresses;
  std::vector<std::string> ethernet_addresses;
  std::vector<std::string> other_addresses;

  try {
    auto host_names = NetworkInformation::GetHostNames();
    for (const auto& host_name : host_names) {
      if (host_name.IPInformation() != nullptr &&
          host_name.IPInformation().NetworkAdapter() != nullptr &&
          host_name.Type() == HostNameType::Ipv4) {
        NetworkAdapter adapter = host_name.IPInformation().NetworkAdapter();
        if (adapter.NetworkItem().GetNetworkTypes() == NetworkTypes::None) {
          // If we're not connected to a network, we don't want to add this
          // address.
          continue;
        }
        if (adapter.IanaInterfaceType() == Constants::kInterfaceTypeWifi) {
          wifi_addresses.push_back(winrt::to_string(host_name.ToString()));
        } else if (adapter.IanaInterfaceType() ==
                   Constants::kInterfaceTypeEthernet) {
          ethernet_addresses.push_back(winrt::to_string(host_name.ToString()));
        } else {
          other_addresses.push_back(winrt::to_string(host_name.ToString()));
        }
      }
    }
  } catch (std::exception exception) {
    LOG(ERROR) << __func__ << ": Cannot get IPv4 addresses. Exception : "
               << exception.what();
  } catch (const winrt::hresult_error& error) {
    LOG(ERROR) << __func__ << ": Cannot get IPv4 addresses. WinRT exception: "
               << error.code() << ": " << winrt::to_string(error.message());
  } catch (...) {
    LOG(ERROR) << __func__ << ": Unknown exeption.";
  }

  result.insert(result.end(), wifi_addresses.begin(), wifi_addresses.end());
  result.insert(result.end(), ethernet_addresses.begin(),
                ethernet_addresses.end());
  result.insert(result.end(), other_addresses.begin(), other_addresses.end());

  return result;
}

std::vector<std::string> Get4BytesIpv4Addresses() {
  std::vector<std::string> result;
  std::vector<std::string> ipv4_addresses = GetIpv4Addresses();
  for (const auto& ipv4_address : ipv4_addresses) {
    // Converts IP address from x.x.x.x to 4 bytes format.
    in_addr address;
    address.S_un.S_addr = inet_addr(ipv4_address.c_str());
    std::string ipv4_4bytes_address;
    ipv4_4bytes_address.resize(4);
    ipv4_4bytes_address[0] = address.S_un.S_un_b.s_b1;
    ipv4_4bytes_address[1] = address.S_un.S_un_b.s_b2;
    ipv4_4bytes_address[2] = address.S_un.S_un_b.s_b3;
    ipv4_4bytes_address[3] = address.S_un.S_un_b.s_b4;
    result.push_back(ipv4_4bytes_address);
  }

  return result;
}

Uuid winrt_guid_to_nearby_uuid(const ::winrt::guid& guid) {
  int64_t data1 = guid.Data1;
  int64_t data2 = guid.Data2;
  int64_t data3 = guid.Data3;

  int64_t msb = ((data1 >> 24) & 0xff) << 56 | ((data1 >> 16) & 0xff) << 48 |
                ((data1 >> 8) & 0xff) << 40 | ((data1) & 0xff) << 32 |
                ((data2 >> 8) & 0xff) << 24 | ((data2) & 0xff) << 16 |
                ((data3 >> 8) & 0xff) << 8 | (data3 & 0xff);

  int64_t lsb =
      ((int64_t)guid.Data4[0]) << 56 | ((int64_t)guid.Data4[1]) << 48 |
      ((int64_t)guid.Data4[2]) << 40 | ((int64_t)guid.Data4[3]) << 32 |
      ((int64_t)guid.Data4[4]) << 24 | ((int64_t)guid.Data4[5]) << 16 |
      ((int64_t)guid.Data4[6]) << 8 | (int64_t)guid.Data4[7];

  return Uuid(msb, lsb);
}

winrt::guid nearby_uuid_to_winrt_guid(Uuid uuid) {
  winrt::guid guid;
  uint64_t msb = uuid.GetMostSigBits();
  guid.Data1 = ((msb >> 56) & 0xff) << 24 | ((msb >> 48) & 0xff) << 16 |
               ((msb >> 40) & 0xff) << 8 | ((msb >> 32) & 0xff);
  guid.Data2 = ((msb >> 24) & 0xff) << 8 | ((msb >> 16) & 0xff);
  guid.Data3 = ((msb >> 8) & 0xff) << 8 | (msb & 0xff);
  uint64_t lsb = uuid.GetLeastSigBits();
  guid.Data4[0] = (lsb >> 56) & 0xff;
  guid.Data4[1] = (lsb >> 48) & 0xff;
  guid.Data4[2] = (lsb >> 40) & 0xff;
  guid.Data4[3] = (lsb >> 32) & 0xff;
  guid.Data4[4] = (lsb >> 24) & 0xff;
  guid.Data4[5] = (lsb >> 16) & 0xff;
  guid.Data4[6] = (lsb >> 8) & 0xff;
  guid.Data4[7] = lsb & 0xff;
  return guid;
}

bool is_nearby_uuid_equal_to_winrt_guid(const Uuid& uuid,
                                        const ::winrt::guid& guid) {
  return uuid == winrt_guid_to_nearby_uuid(guid);
}

ByteArray Sha256(absl::string_view input, size_t size) {
  ByteArray hash = nearby::Crypto::Sha256(input);
  return ByteArray{hash.data(), size};
}

bool InspectableReader::ReadBoolean(IInspectable inspectable) {
  if (inspectable == nullptr) {
    return false;
  }

  auto property_value =
      inspectable.try_as<winrt::Windows::Foundation::IPropertyValue>();
  if (property_value == nullptr) {
    throw std::invalid_argument("no property value interface.");
  }
  if (property_value.Type() !=
      winrt::Windows::Foundation::PropertyType::Boolean) {
    throw std::invalid_argument("not uin16 data type.");
  }

  return property_value.GetBoolean();
}

uint16_t InspectableReader::ReadUint16(IInspectable inspectable) {
  if (inspectable == nullptr) {
    return 0;
  }

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

uint32_t InspectableReader::ReadUint32(IInspectable inspectable) {
  if (inspectable == nullptr) {
    return 0;
  }

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
  if (inspectable == nullptr) {
    return "";
  }

  auto property_value =
      inspectable.try_as<winrt::Windows::Foundation::IPropertyValue>();
  if (property_value == nullptr) {
    throw std::invalid_argument("no property value interface.");
  }
  if (property_value.Type() !=
      winrt::Windows::Foundation::PropertyType::String) {
    throw std::invalid_argument("not string data type.");
  }

  return nearby::windows::string_utils::WideStringToString(
      property_value.GetString().c_str());
}

std::vector<std::string> InspectableReader::ReadStringArray(
    IInspectable inspectable) {
  std::vector<std::string> result;
  if (inspectable == nullptr) {
    return result;
  }

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

  for (const winrt::hstring& str : strings) {
    result.push_back(winrt::to_string(str));
  }
  return result;
}

}  // namespace windows
}  // namespace nearby
