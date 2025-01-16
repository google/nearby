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
#include <list>
#include <memory>
#include <utility>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "sharing/internal/api/mock_network_monitor.h"
#include "sharing/internal/api/mock_sharing_platform.h"
#include "sharing/internal/api/mock_system_info.h"
#include "sharing/internal/api/system_info.h"
#include "sharing/internal/public/connectivity_manager.h"

namespace nearby {
namespace {
using ::nearby::sharing::api::MockNetworkMonitor;
using ::nearby::sharing::api::MockSharingPlatform;
using ::nearby::sharing::api::MockSystemInfo;
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

TEST(ConnectivityManagerImpl, IsHPRealtekDeviceReturnsTrue) {
  MockSharingPlatform sharing_platform;
  auto system_info = std::make_unique<MockSystemInfo>();
  auto system_info_ptr = system_info.get();
  EXPECT_CALL(sharing_platform, CreateSystemInfo)
      .WillOnce(Return(std::move(system_info)));
  std::list<api::SystemInfo::DriverInfo> driver_infos = {
      {.manufacturer = "realtek Semiconductors"}, {.manufacturer = "Intel"}};
  EXPECT_CALL(*system_info_ptr, GetNetworkDriverInfos)
      .WillOnce(Return(driver_infos));
  EXPECT_CALL(*system_info_ptr, GetComputerManufacturer).WillOnce(Return("hp"));

  ConnectivityManagerImpl connectivity_manager_impl(sharing_platform);
  EXPECT_TRUE(connectivity_manager_impl.IsHPRealtekDevice());
}

TEST(ConnectivityManagerImpl, IsHPRealtekDevice_NotHPDevice) {
  MockSharingPlatform sharing_platform;
  auto system_info = std::make_unique<MockSystemInfo>();
  auto system_info_ptr = system_info.get();
  EXPECT_CALL(sharing_platform, CreateSystemInfo)
      .WillOnce(Return(std::move(system_info)));
  std::list<api::SystemInfo::DriverInfo> driver_infos = {
      {.manufacturer = "realtek Semiconductors"}, {.manufacturer = "Intel"}};
  EXPECT_CALL(*system_info_ptr, GetNetworkDriverInfos)
      .WillOnce(Return(driver_infos));
  EXPECT_CALL(*system_info_ptr, GetComputerManufacturer)
      .WillOnce(Return("Lenovo"));

  ConnectivityManagerImpl connectivity_manager_impl(sharing_platform);
  EXPECT_FALSE(connectivity_manager_impl.IsHPRealtekDevice());
}

TEST(ConnectivityManagerImpl, IsHPRealtekDevice_NotRealtekDevice) {
  MockSharingPlatform sharing_platform;
  auto system_info = std::make_unique<MockSystemInfo>();
  auto system_info_ptr = system_info.get();
  EXPECT_CALL(sharing_platform, CreateSystemInfo)
      .WillOnce(Return(std::move(system_info)));
  std::list<api::SystemInfo::DriverInfo> driver_infos = {
      {.manufacturer = "Intel"}};
  EXPECT_CALL(*system_info_ptr, GetNetworkDriverInfos)
      .WillOnce(Return(driver_infos));

  ConnectivityManagerImpl connectivity_manager_impl(sharing_platform);
  EXPECT_FALSE(connectivity_manager_impl.IsHPRealtekDevice());
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
