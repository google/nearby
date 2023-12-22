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

#include "sharing/internal/public/connectivity_manager_impl.h"

#include <functional>
#include <memory>
#include <utility>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "sharing/internal/api/mock_network_monitor.h"
#include "sharing/internal/api/mock_sharing_platform.h"
#include "sharing/internal/public/connectivity_manager.h"

namespace nearby {
namespace {
using ::nearby::sharing::api::MockNetworkMonitor;
using ::nearby::sharing::api::MockSharingPlatform;
using ::testing::_;
using ::testing::ByMove;
using ::testing::Return;

TEST(ConnectivityManagerImpl, IsLanConnected) {
  MockSharingPlatform sharing_platform;
  auto network_monitor = std::make_unique<MockNetworkMonitor>();
  MockNetworkMonitor* mock_network_monitor = network_monitor.get();
  EXPECT_CALL(*mock_network_monitor, IsLanConnected()).WillOnce(Return(true));
  EXPECT_CALL(sharing_platform, CreateNetworkMonitor(_))
      .WillOnce(Return(ByMove(std::move(network_monitor))));

  ConnectivityManagerImpl connectivity_manager_impl(sharing_platform);
  EXPECT_TRUE(connectivity_manager_impl.IsLanConnected());
}

TEST(ConnectivityManagerImpl, GetConnectionType) {
  MockSharingPlatform sharing_platform;
  auto network_monitor = std::make_unique<MockNetworkMonitor>();
  MockNetworkMonitor* mock_network_monitor = network_monitor.get();
  EXPECT_CALL(sharing_platform, CreateNetworkMonitor(_))
      .WillOnce(Return(ByMove(std::move(network_monitor))));
  EXPECT_CALL(*mock_network_monitor, GetCurrentConnection())
      .WillOnce(Return(MockNetworkMonitor::ConnectionType::kWifi));

  ConnectivityManagerImpl connectivity_manager_impl(sharing_platform);
  EXPECT_EQ(connectivity_manager_impl.GetConnectionType(),
            ConnectivityManager::ConnectionType::kWifi);
}

TEST(ConnectivityManagerImpl, RegisterConnectionListener) {
  std::function<void(ConnectivityManager::ConnectionType, bool)> listener_1 =
      [](ConnectivityManager::ConnectionType connection_type,
         bool is_lan_connected) {};
  std::function<void(ConnectivityManager::ConnectionType, bool)> listener_2 =
      [](ConnectivityManager::ConnectionType connection_type,
         bool is_lan_connected) {};

  MockSharingPlatform sharing_platform;
  ConnectivityManagerImpl connectivity_manager_impl(sharing_platform);
  EXPECT_EQ(connectivity_manager_impl.GetListenerCount(), 0);

  connectivity_manager_impl.RegisterConnectionListener("listener_1",
                                                       listener_1);
  EXPECT_EQ(connectivity_manager_impl.GetListenerCount(), 1);

  connectivity_manager_impl.RegisterConnectionListener("listener_2",
                                                       listener_2);
  EXPECT_EQ(connectivity_manager_impl.GetListenerCount(), 2);
}

TEST(ConnectivityManagerImpl, UnregisterConnectionListener) {
  std::function<void(ConnectivityManager::ConnectionType, bool)> listener_1 =
      [](ConnectivityManager::ConnectionType connection_type,
         bool is_lan_connected) {};
  std::function<void(ConnectivityManager::ConnectionType, bool)> listener_2 =
      [](ConnectivityManager::ConnectionType connection_type,
         bool is_lan_connected) {};

  MockSharingPlatform sharing_platform;
  ConnectivityManagerImpl connectivity_manager_impl(sharing_platform);
  connectivity_manager_impl.RegisterConnectionListener("listener_1",
                                                       listener_1);
  connectivity_manager_impl.RegisterConnectionListener("listener_2",
                                                       listener_2);

  EXPECT_EQ(connectivity_manager_impl.GetListenerCount(), 2);

  connectivity_manager_impl.UnregisterConnectionListener("listener_1");
  EXPECT_EQ(connectivity_manager_impl.GetListenerCount(), 1);

  connectivity_manager_impl.UnregisterConnectionListener("listener_2");
  EXPECT_EQ(connectivity_manager_impl.GetListenerCount(), 0);
}

}  // namespace
}  // namespace nearby
