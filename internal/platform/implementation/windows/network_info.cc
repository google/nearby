// Copyright 2025 Google LLC
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

#include "internal/platform/implementation/windows/network_info.h"

// clang-format off
#include <winsock2.h>
#include <iphlpapi.h>
// clang-format on

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/base/no_destructor.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/windows/socket_address.h"
#include "internal/platform/implementation/windows/string_utils.h"
#include "internal/platform/implementation/windows/wlan_client.h"
#include "internal/platform/logging.h"

namespace nearby::windows {

/* static */
void NetworkInfo::AddIpUnicastAddresses(
    IP_ADAPTER_UNICAST_ADDRESS* unicast_addresses,
    NetworkInfo::InterfaceInfo& net_interface) {
  while (unicast_addresses != nullptr) {
    sockaddr* address = unicast_addresses->Address.lpSockaddr;
    if (address == nullptr) {
      unicast_addresses = unicast_addresses->Next;
      continue;
    }
    SocketAddress socket_address;
    std::memcpy(socket_address.address(), address,
                unicast_addresses->Address.iSockaddrLength);
    if (socket_address.family() == AF_INET) {
      net_interface.ipv4_addresses.push_back(std::move(socket_address));
    } else if (socket_address.family() == AF_INET6) {
      net_interface.ipv6_addresses.push_back(std::move(socket_address));
    }
    unicast_addresses = unicast_addresses->Next;
  }
  LOG(INFO) << "Added to interface: " << net_interface.index << ", "
            << net_interface.ipv4_addresses.size() << " v4 addresses, "
            << net_interface.ipv6_addresses.size() << " v6 addresses.";
}

NetworkInfo& NetworkInfo::GetNetworkInfo() {
  static absl::NoDestructor<NetworkInfo> kNetworkInfo;
  return *kNetworkInfo;
}

void NetworkInfo::GetWifiLuids(std::vector<ULONG64>& wifi_luids) {
  // If `wifi_luids` is not empty, it is assumed to be populated already.
  if (!wifi_luids.empty()) {
    return;
  }
  if (!wlan_client_.Initialize()) {
    return;
  }
  auto wlan_interfaces = wlan_client_.GetInterfaceInfos();
  for (const auto& wlan_interface : wlan_interfaces) {
    NET_LUID luid;
    if (ConvertInterfaceGuidToLuid(&wlan_interface.guid, &luid) == NO_ERROR) {
      wifi_luids.push_back(luid.Value);
    }
  }
}

bool NetworkInfo::Refresh() {
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
    error_code =
        ::GetAdaptersAddresses(AF_UNSPEC, kDefaultFlags, /*reserved=*/nullptr,
                               addresses, &buffer_size);
  } while (error_code == ERROR_BUFFER_OVERFLOW &&
           buffer_size <= kMaxBufferSize);
  if (error_code != ERROR_NO_DATA && error_code != NO_ERROR) {
    LOG(ERROR) << "Cannot get adapter addresses. Error code: " << error_code;
    return false;
  }
  if (error_code == ERROR_NO_DATA) {
    LOG(INFO) << "No network interfaces found.";
    absl::MutexLock lock(mutex_);
    interfaces_.clear();
    return true;
  }
  std::vector<InterfaceInfo> result;
  IP_ADAPTER_ADDRESSES* next_address = addresses;
  std::vector<ULONG64> wifi_luids;
  while (next_address != nullptr) {
    if (next_address->OperStatus != IfOperStatusUp ||
        next_address->IfType == IF_TYPE_SOFTWARE_LOOPBACK ||
        next_address->FirstUnicastAddress == nullptr) {
      // Skip down and loopback interfaces as well as interfaces without
      // addresses.
      next_address = next_address->Next;
      continue;
    }
    auto it = result.insert(result.end(), InterfaceInfo{
                                              .index = next_address->IfIndex,
                                              .luid = next_address->Luid,
                                          });
    if (next_address->IfType == IF_TYPE_ETHERNET_CSMACD) {
      it->type = InterfaceType::kEthernet;
      VLOG(1) << "Found ethernet interface: "
              << string_utils::WideStringToString(next_address->Description)
              << ", index: " << next_address->IfIndex;
    } else if (next_address->IfType == IF_TYPE_IEEE80211) {
      // Hotspot interfaces are not available from WlanClient, so if we can't
      // find the interface in the list, we assume it's a hotspot interface.
      GetWifiLuids(wifi_luids);
      if (absl::c_find(wifi_luids,
                    next_address->Luid.Value) == wifi_luids.end()) {
        it->type = InterfaceType::kWifiHotspot;
        VLOG(1) << "Found wifi-hotspot interface: "
                << string_utils::WideStringToString(next_address->Description)
                << ", index: " << next_address->IfIndex;
      } else {
        it->type = InterfaceType::kWifi;
        VLOG(1) << "Found wifi interface: "
                << string_utils::WideStringToString(next_address->Description)
                << ", index: " << next_address->IfIndex;
      }
    } else {
      it->type = InterfaceType::kOther;
      VLOG(1) << "Found other interface: "
              << string_utils::WideStringToString(next_address->Description)
              << ", index: " << next_address->IfIndex;
    }
    AddIpUnicastAddresses(next_address->FirstUnicastAddress, *it);
    next_address = next_address->Next;
  }
  LOG(INFO) << "Found " << result.size() << " up interfaces.";
  {
    absl::MutexLock lock(mutex_);
    interfaces_ = std::move(result);
  }
  return true;
}

std::vector<NetworkInfo::InterfaceInfo> NetworkInfo::GetInterfaces() const {
  absl::MutexLock lock(mutex_);
  return interfaces_;
}

bool NetworkInfo::RenewIpv4Address(NET_LUID luid) const {
  uint64_t index = 0;
  {
    absl::MutexLock lock(mutex_);
    auto it = std::find_if(interfaces_.begin(), interfaces_.end(),
                           [luid](const InterfaceInfo& intf) {
                             return intf.luid.Value == luid.Value;
                           });
    if (it == interfaces_.end()) {
      LOG(ERROR) << "Interface not found: " << absl::Hex(luid.Value);
      return false;
    }
    index = it->index;
  }
  ULONG interface_info_size = 0;
  DWORD error = ::GetInterfaceInfo(nullptr, &interface_info_size);
  if (error == ERROR_NO_DATA || error == NO_ERROR) {
    LOG(INFO) << "No interface info found.";
    return true;
  }
  if (error != ERROR_INSUFFICIENT_BUFFER) {
    LOG(ERROR) << "GetInterfaceInfo failed: " << error;
    return false;
  }
  std::string interface_info_data;
  interface_info_data.resize(interface_info_size);
  IP_INTERFACE_INFO* interface_info =
      reinterpret_cast<IP_INTERFACE_INFO*>(interface_info_data.data());
  error = ::GetInterfaceInfo(interface_info, &interface_info_size);
  if (error != NO_ERROR && error != ERROR_NO_DATA) {
    LOG(ERROR) << "GetInterfaceInfo failed: " << error;
    return false;
  }
  error = NO_ERROR;
  VLOG(1) << "Got " << interface_info->NumAdapters << " IPV4 adapters";
  for (int i = 0; i < interface_info->NumAdapters; ++i) {
    if (interface_info->Adapter[i].Index != index) {
      continue;
    }
    LOG(INFO) << "Renewing IPV4 address for adapter: " << index;
    error = ::IpRenewAddress(&interface_info->Adapter[i]);
    VLOG(1) << "Renewed IPV4 address for adapter: "
            << interface_info->Adapter[i].Index;
  }
  if (error != NO_ERROR) {
    LOG(ERROR) << "IpRenewAddress failed: " << error;
    return false;
  }
  return true;
}

}  // namespace nearby::windows
