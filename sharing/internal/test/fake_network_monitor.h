// Copyright 2022 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_SHARING_INTERNAL_TEST_FAKE_NETWORK_MONITOR_H_
#define THIRD_PARTY_NEARBY_SHARING_INTERNAL_TEST_FAKE_NETWORK_MONITOR_H_

#include <functional>

#include "sharing/internal/api/network_monitor.h"

namespace nearby {

class FakeNetworkMonitor : public api::NetworkMonitor {
 public:
  explicit FakeNetworkMonitor(
      std::function<void(api::NetworkMonitor::ConnectionType, bool)> callback)
      : api::NetworkMonitor(callback) {}

  ~FakeNetworkMonitor() override { callback_ = nullptr; }

  bool IsLanConnected() override { return is_lan_connected_; }

  api::NetworkMonitor::ConnectionType GetCurrentConnection() override {
    return api::NetworkMonitor::ConnectionType::kWifi;
  }

  void SetLanConnected(bool connected) { is_lan_connected_ = connected; }

  void TestNetworkChangeToEthernet() {
    callback_(api::NetworkMonitor::ConnectionType::kEthernet,
              is_lan_connected_);
  }

 private:
  bool is_lan_connected_ = true;
};

}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_TEST_FAKE_NETWORK_MONITOR_H_
