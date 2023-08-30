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

#include "internal/platform/implementation/linux/utils.h"

// Standard C/C++ headers
#include <cerrno>
#include <codecvt>
#include <cstdint>
#include <exception>
#include <stdexcept>
#include <string>
#include <vector>

// Third party headers
#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"

// Nearby connections headers
#include "absl/strings/string_view.h"
#include "internal/platform/bluetooth_utils.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/implementation/crypto.h"
#include "internal/platform/logging.h"
#include "internal/platform/uuid.h"

// Linux headers
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <netinet/in.h>

namespace nearby {
namespace linux {
namespace {

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
  union addrs {
    in_addr_t addr;
    uint8_t bits[4];
  } address;

  address.bits[0] = ipaddr_4bytes[0];
  address.bits[1] = ipaddr_4bytes[1];
  address.bits[2] = ipaddr_4bytes[2];
  address.bits[3] = ipaddr_4bytes[3];

  struct in_addr addr;

  addr.s_addr = address.addr;
  char* ipv4_address = inet_ntoa(addr);
  if (ipv4_address == nullptr) {
    return {};
  }

  return std::string(ipv4_address);
}

std::string ipaddr_dotdecimal_to_4bytes_string(std::string ipv4_s) {
  if (ipv4_s.empty()) {
    return {};
  }

  struct in_addr addr;

  if (inet_aton(ipv4_s.c_str(), &addr) != 0) {
    return {};
  }

  std::string ipv4_b = std::to_string(addr.s_addr);

  return std::string();
}

std::wstring string_to_wstring(std::string str) {
  std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
  return converter.from_bytes(str);
}

std::string wstring_to_string(std::wstring wstr) {
  std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
  return converter.to_bytes(wstr);
}

std::vector<std::string> GetIpv4Addresses() {
  std::vector<std::string> result;

  struct ifaddrs* interface = nullptr;
  char host[NI_MAXHOST];

  if (getifaddrs(&interface) != 0) {
    NEARBY_LOGS(ERROR) << __func__ << ": Failed to get interfaces. Error: "
                       << strerror(errno);
    freeifaddrs(interface);
    return {};
  }
  int status = 0;
  for (struct ifaddrs* ifa = interface; ifa != nullptr; ifa = ifa->ifa_next) {
    if (ifa->ifa_addr->sa_family == AF_INET) {
      status = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host,
                           NI_MAXHOST, nullptr, 0, NI_NUMERICHOST);
    }
    switch (status) {
      case EAI_AGAIN:
        NEARBY_LOGS(ERROR) << __func__ << "Failed to get IP for interface: "
                           << ifa->ifa_name
                           << " : The name could not be resolved at this time. "
                           << "Try again later.";
        break;
      case EAI_BADFLAGS:
        NEARBY_LOGS(ERROR) << __func__ << "Failed to get IP for interface: "
                           << ifa->ifa_name
                           << " : The flags argument has an invalid value.";
        break;
      case EAI_FAIL:
        NEARBY_LOGS(ERROR) << __func__ << "Failed to get IP for interface: "
                           << ifa->ifa_name
                           << " : A nonrecoverable error occured.";
        break;
      case EAI_FAMILY:
        NEARBY_LOGS(ERROR) << __func__ << "Failed to get IP for interface: "
                           << ifa->ifa_name
                           << " : The address family was not recognized, "
                           << "or the address length was invalid for the "
                           << "specified family.";
        break;
      case EAI_MEMORY:
        NEARBY_LOGS(ERROR) << __func__ << "Failed to get IP for interface: "
                           << ifa->ifa_name << " : Out of memory.";
        break;
      case EAI_NONAME:
        NEARBY_LOGS(ERROR)
            << __func__ << "Failed to get IP for interface: " << ifa->ifa_name
            << " : The name does not resolve for the suplied arguments."
            << " NI_NAMEREQD is set and the host's name cannot be located, "
            << "or neither hostname nor service name were requsted.";
        break;
      case EAI_OVERFLOW:
        NEARBY_LOGS(ERROR)
            << __func__ << "Failed to get IP for interface: " << ifa->ifa_name
            << " : The bugger pointed to by `host` or `serv` was too small.";
        break;
      case EAI_SYSTEM:
        NEARBY_LOGS(ERROR) << __func__
                           << "A system error occured. Error code: " << errno
                           << ": " << strerror(errno);
        break;
    }
  }
  freeifaddrs(interface);
  return result;
}

std::vector<std::string> Get4BytesIpv4Addresses() {
  std::vector<std::string> result;
  std::vector<std::string> ipv4_addresses = GetIpv4Addresses();
  for (const auto& ipv4_address : ipv4_addresses) {
    // Converts IP address from x.x.x.x to 4 bytes format using utils function
    result.push_back(ipaddr_dotdecimal_to_4bytes_string(ipv4_address));
  }

  return result;
}

/*
Uuid winrt_guid_to_nearby_uuid(const ::winrt::guid& guid) {
  int64_t data1 = guid.Data1;
  int64_t data2 = guid.Data2;
  int64_t data3 = guid.Data3;

  int64_t msb = ((data1 >> 24) & 0xff) << 56 | ((data1 >> 16) & 0xff) << 48 |
                ((data1 >> 8) & 0xff) << 40 | ((data1)&0xff) << 32 |
                ((data2 >> 8) & 0xff) << 24 | ((data2)&0xff) << 16 |
                ((data3 >> 8) & 0xff) << 8 | (data3 & 0xff);

  int64_t lsb =
      ((int64_t)guid.Data4[0]) << 56 | ((int64_t)guid.Data4[1]) << 48 |
      ((int64_t)guid.Data4[2]) << 40 | ((int64_t)guid.Data4[3]) << 32 |
      ((int64_t)guid.Data4[4]) << 24 | ((int64_t)guid.Data4[5]) << 16 |
      ((int64_t)guid.Data4[6]) << 8 | (int64_t)guid.Data4[7];

  return Uuid(msb, lsb);
}
*/

/*
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
*/

/*
bool is_nearby_uuid_equal_to_winrt_guid(const Uuid& uuid,
                                     const ::winrt::guid& guid) {
  return uuid == winrt_guid_to_nearby_uuid(guid);
}
*/

ByteArray Sha256(absl::string_view input, size_t size) {
  ByteArray hash = nearby::Crypto::Sha256(input);
  return ByteArray{hash.data(), size};
}
/*
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

uint16 InspectableReader::ReadUint16(IInspectable inspectable) {
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

uint32 InspectableReader::ReadUint32(IInspectable inspectable) {
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

  return wstring_to_string(property_value.GetString().c_str());
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

  for (winrt::hstring str : strings) {
    result.push_back(winrt::to_string(str));
  }
  return result;
}
*/
}  // namespace
}  // namespace linux
}  // namespace nearby
