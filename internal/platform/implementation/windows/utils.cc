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

// clang-format off
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
// clang-format on

// Standard C/C++ headers
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

// Nearby connections headers
#include "absl/strings/string_view.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/implementation/crypto.h"
#include "internal/platform/implementation/windows/string_utils.h"
#include "internal/platform/logging.h"
#include "internal/platform/uuid.h"
#include "winrt/Windows.Foundation.Collections.h"
#include "winrt/Windows.Networking.Connectivity.h"
#include "winrt/base.h"

namespace nearby::windows {
namespace {

void AddIpUnicastAddresses(IP_ADAPTER_UNICAST_ADDRESS* unicast_addresses,
                           std::vector<std::string>& addresses) {
  std::string address;
  while (unicast_addresses != nullptr) {
    DWORD size = INET6_ADDRSTRLEN;  // Max IP address length.
    address.resize(size);
    if (WSAAddressToStringA(unicast_addresses->Address.lpSockaddr,
                            unicast_addresses->Address.iSockaddrLength,
                            /*lpProtocolInfo=*/nullptr, address.data(),
                            &size) != 0) {
      LOG(ERROR) << __func__ << ": Cannot convert address to string.";
      continue;
    }
    address.resize(size);
    addresses.push_back(address);
    unicast_addresses = unicast_addresses->Next;
  }
}

void GetIpAddressesNative(int family, std::vector<std::string>& wifi_addresses,
                          std::vector<std::string>& ethernet_addresses,
                          std::vector<std::string>& other_addresses) {
  static constexpr int kDefaultBufferSize = 15 * 1024;  // default to 15K buffer
  static constexpr int kMaxBufferSize =
      45 * 1024;  // Try to increase buffer 2 times.
  static constexpr ULONG kDefaultFlags =
      GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST |
      GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_SKIP_FRIENDLY_NAME;
  ULONG buffer_size = 0;
  // A string to own the memory for IP_ADAPTER_ADDRESSES.
  std::string address_buffer;
  ULONG error_code = ERROR_NO_DATA;
  IP_ADAPTER_ADDRESSES* addresses = nullptr;
  do {
    buffer_size += kDefaultBufferSize;
    address_buffer.reserve(buffer_size);
    addresses = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(address_buffer.data());
    error_code = GetAdaptersAddresses(
        family, kDefaultFlags, /*reserved=*/nullptr, addresses, &buffer_size);
  } while (error_code == ERROR_BUFFER_OVERFLOW &&
           buffer_size <= kMaxBufferSize);
  if (error_code != ERROR_NO_DATA && error_code != NO_ERROR) {
    LOG(ERROR) << __func__
               << ": Cannot get adapter addresses. Error code: " << error_code;
    return;
  }
  if (error_code == ERROR_NO_DATA) {
    LOG(INFO) << __func__ << ": No IPv4 addresses found.";
    return;
  }
  IP_ADAPTER_ADDRESSES* next_address = addresses;
  while (next_address != nullptr) {
    if (next_address->OperStatus == IfOperStatusUp) {
      if (next_address->IfType == IF_TYPE_ETHERNET_CSMACD) {
        VLOG(1) << "Found ethernet adater: " << next_address->AdapterName
                << " index: " << next_address->IfIndex
                << " v6 index: " << next_address->Ipv6IfIndex;
        AddIpUnicastAddresses(next_address->FirstUnicastAddress,
                              ethernet_addresses);
      } else if (next_address->IfType == IF_TYPE_IEEE80211) {
        VLOG(1) << "Found wifi adapter: " << next_address->AdapterName
                << " index: " << next_address->IfIndex
                << " v6 index: " << next_address->Ipv6IfIndex;
        AddIpUnicastAddresses(next_address->FirstUnicastAddress,
                              wifi_addresses);
      } else if (next_address->IfType != IF_TYPE_SOFTWARE_LOOPBACK) {
        // Skip loopback interfaces.
        VLOG(1) << "Found other adapter: " << next_address->AdapterName;
        AddIpUnicastAddresses(next_address->FirstUnicastAddress,
                              other_addresses);
      }
    }
    next_address = next_address->Next;
  }
}

}  // namespace

std::string ipaddr_4bytes_to_dotdecimal_string(
    absl::string_view ipaddr_4bytes) {
  if (ipaddr_4bytes.size() != 4) {
    return {};
  }

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

void GetIpv4Addresses(std::vector<std::string>& wifi_addresses,
                      std::vector<std::string>& ethernet_addresses,
                      std::vector<std::string>& other_addresses) {
  GetIpAddressesNative(AF_INET, wifi_addresses, ethernet_addresses,
                        other_addresses);
}

std::vector<std::string> GetIpv4Addresses() {
  std::vector<std::string> result;
  GetIpv4Addresses(result, result, result);
  return result;
}

std::vector<std::string> GetWifiIpv4Addresses() {
  std::vector<std::string> result;
  std::vector<std::string> ethernet_addresses;
  std::vector<std::string> other_addresses;
  GetIpv4Addresses(result, ethernet_addresses, other_addresses);
  return result;
}

void GetConnectedNetworks(bool& is_wifi_connected, bool& is_ethernet_connected,
                          bool& is_other_connected) {
  std::vector<std::string> wifi_addresses;
  std::vector<std::string> ethernet_addresses;
  std::vector<std::string> other_addresses;
  GetIpv4Addresses(wifi_addresses, ethernet_addresses, other_addresses);
  is_wifi_connected = !wifi_addresses.empty();
  is_ethernet_connected = !ethernet_addresses.empty();
  is_other_connected = !other_addresses.empty();
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

std::optional<std::wstring> GetDnsHostName() {
  DWORD size = 0;

  // Get length of the computer name.
  if (GetComputerNameExW(ComputerNameDnsHostname, nullptr, &size) == 0) {
    if (GetLastError() != ERROR_MORE_DATA) {
      LOG(ERROR) << ": Failed to get device dns name size, error:"
                 << GetLastError();
      return std::nullopt;
    }
  }
  std::wstring device_name(size, L' ');
  if (GetComputerNameExW(ComputerNameDnsHostname, device_name.data(), &size) !=
      0) {
    // On input size includes null termination.
    // On output size excludes null termination.
    device_name.resize(size);
    return device_name;
  }

  LOG(ERROR) << ": Failed to get device dns name, error:" << GetLastError();
  return std::nullopt;
}

}  // namespace nearby::windows
