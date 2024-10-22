// Copyright 2021 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_SHARING_INTERNAL_PUBLIC_CONNECTIVITY_MANAGER_IMPL_H_
#define THIRD_PARTY_NEARBY_SHARING_INTERNAL_PUBLIC_CONNECTIVITY_MANAGER_IMPL_H_

#include <functional>
#include <memory>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "sharing/internal/api/network_monitor.h"
#include "sharing/internal/api/sharing_platform.h"
#include "sharing/internal/public/connectivity_manager.h"

namespace nearby {

// Limitation: this is not thread safe and needs to be enhanced
class ConnectivityManagerImpl : public ConnectivityManager {
 public:
  explicit ConnectivityManagerImpl(
      nearby::sharing::api::SharingPlatform& platform);

  bool IsLanConnected() override;

  ConnectionType GetConnectionType() override;

  void RegisterConnectionListener(
      absl::string_view listener_name,
      std::function<void(ConnectionType, bool)> callback) override;
  void UnregisterConnectionListener(absl::string_view listener_name) override;

  int GetListenerCount() const;

 private:
  absl::flat_hash_map<std::string, std::function<void(ConnectionType, bool)>>
      listeners_;
  std::unique_ptr<api::NetworkMonitor> network_monitor_;
};

}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_PUBLIC_CONNECTIVITY_MANAGER_IMPL_H_
