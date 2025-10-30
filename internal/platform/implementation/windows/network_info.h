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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_NETWORK_INFO_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_NETWORK_INFO_H_

// clang-format off
#include <winsock2.h>
#include <ifdef.h>
// clang-format on

#include <cstdint>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/windows/wlan_client.h"

namespace nearby::windows {

// The type of the network interface.
enum InterfaceType {
  kEthernet,
  kWifi,
  kWifiHotspot,  // Wifi SoftAP interface
  kOther,
};

// Class to track network interfaces details.  These include the interface type,
// interface index, NET_LUID and IP addresses.
// This class is thread-safe.
class NetworkInfo {
 public:
  struct InterfaceInfo {
    uint64_t index;
    InterfaceType type;
    NET_LUID luid;
    std::vector<sockaddr_storage> ipv4_addresses;
    std::vector<sockaddr_storage> ipv6_addresses;
  };

  // Returns the singleton instance of this class.
  static NetworkInfo& GetNetworkInfo();

  // Refreshes the network interfaces information keep by this class.
  // Returns true on success.
  bool Refresh();
  // Returns the network interfaces information.
  std::vector<InterfaceInfo> GetInterfaces() const;
  // Renews the IPv4 address for the given interface.
  // Returns true on success.
  bool RenewIpv4Address(NET_LUID luid) const;

 private:
  // Lazily initializes the list of wlan interface LUIDs.  If `wifi_luids` is
  // not empty, it is assumed to be populated already.
  void GetWifiLuids(std::vector<ULONG64>& wifi_luids);

  WlanClient wlan_client_;
  mutable absl::Mutex mutex_;
  std::vector<InterfaceInfo> interfaces_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace nearby::windows

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_NETWORK_INFO_H_
