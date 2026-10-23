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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_MOCK_WIFI_LAN_MEDIUM_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_MOCK_WIFI_LAN_MEDIUM_H_

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "absl/types/optional.h"
#include "gmock/gmock.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/implementation/upgrade_address_info.h"
#include "internal/platform/implementation/wifi_lan.h"
#include "internal/platform/nsd_service_info.h"
#include "internal/platform/service_address.h"

namespace nearby {

class MockWifiLanMedium : public api::WifiLanMedium {
 public:
  MOCK_METHOD(bool, IsNetworkConnected, (), (const, override));
  MOCK_METHOD(bool, StartAdvertising, (const NsdServiceInfo& nsd_service_info),
              (override));
  MOCK_METHOD(bool, StopAdvertising, (const NsdServiceInfo& nsd_service_info),
              (override));
  MOCK_METHOD(bool, StartDiscovery,
              (const std::string& service_type,
               DiscoveredServiceCallback callback),
              (override));
  MOCK_METHOD(bool, StopDiscovery, (const std::string& service_type),
              (override));
  MOCK_METHOD(std::unique_ptr<api::WifiLanSocket>, ConnectToService,
              (const NsdServiceInfo& remote_service_info,
               CancellationFlag* cancellation_flag),
              (override));
  MOCK_METHOD(std::unique_ptr<api::WifiLanSocket>, ConnectToService,
              (const ServiceAddress& service_address,
               CancellationFlag* cancellation_flag),
              (override));
  MOCK_METHOD(std::unique_ptr<api::WifiLanServerSocket>, ListenForService,
              (int port), (override));
  MOCK_METHOD((absl::optional<std::pair<std::int32_t, std::int32_t>>),
              GetDynamicPortRange, (), (override));
  MOCK_METHOD(api::UpgradeAddressInfo, GetUpgradeAddressCandidates,
              (const api::WifiLanServerSocket& server_socket), (override));
};

}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_MOCK_WIFI_LAN_MEDIUM_H_
