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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_UPGRADE_ADDRESS_INFO_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_UPGRADE_ADDRESS_INFO_H_

#include <vector>


#include "internal/platform/service_address.h"

namespace nearby::api {

// Container for upgrade address candidates and network configuration info.
struct UpgradeAddressInfo {
  // Number of network interfaces that can be used for upgrade.
  int num_interfaces = 0;
  // Number of network interfaces for upgrade that are IPv6 only.
  int num_ipv6_only_interfaces = 0;
  // Address list is sorted so IPv6 addresses are first.
  std::vector<ServiceAddress> address_candidates;
};

}  // namespace nearby::api

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_UPGRADE_ADDRESS_INFO_H_
