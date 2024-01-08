// Copyright 2023 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_MOCK_NETWORK_MONITOR_H_
#define THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_MOCK_NETWORK_MONITOR_H_

#include "sharing/internal/api/network_monitor.h"

#include "gmock/gmock.h"

namespace nearby::sharing::api {

class MockNetworkMonitor : public nearby::api::NetworkMonitor {
 public:
  MockNetworkMonitor() : nearby::api::NetworkMonitor(nullptr) {}
  MockNetworkMonitor(const MockNetworkMonitor&) = delete;
  MockNetworkMonitor& operator=(const MockNetworkMonitor&) = delete;
  ~MockNetworkMonitor() override = default;

  MOCK_METHOD(bool, IsLanConnected, (), (override));

  MOCK_METHOD(ConnectionType, GetCurrentConnection, (), (override));
};

}  // namespace nearby::sharing::api

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_MOCK_NETWORK_MONITOR_H_
