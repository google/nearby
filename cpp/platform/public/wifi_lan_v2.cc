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

#include "platform/public/wifi_lan_v2.h"

#include "platform/public/mutex_lock.h"

namespace location {
namespace nearby {

bool WifiLanMediumV2::StartAdvertising(const NsdServiceInfo& nsd_service_info) {
  return impl_->StartAdvertising(nsd_service_info);
}

bool WifiLanMediumV2::StopAdvertising(const NsdServiceInfo& nsd_service_info) {
  return impl_->StopAdvertising(nsd_service_info);
}

bool WifiLanMediumV2::StartDiscovery(const std::string& service_type,
                                     DiscoveredServiceCallback callback) {
  return false;
}

bool WifiLanMediumV2::StopDiscovery(const std::string& service_type) {
  return false;
}

WifiLanSocketV2 WifiLanMediumV2::ConnectToService(
    NsdServiceInfo remote_service_info, CancellationFlag* cancellation_flag) {
  return {};
}

WifiLanSocketV2 WifiLanMediumV2::ConnectToService(
    const std::string& ip_address, int port,
    CancellationFlag* cancellation_flag) {
  return {};
}

}  // namespace nearby
}  // namespace location
