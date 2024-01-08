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

#include "sharing/internal/test/fake_connectivity_manager.h"

#include <string>

#include "gtest/gtest.h"
#include "sharing/internal/public/connectivity_manager.h"

namespace nearby {
namespace {

TEST(FakeConnectivityManager, TestIsLanConnected) {
  FakeConnectivityManager connection_manager;
  bool is_lan_connected = connection_manager.IsLanConnected();
  ASSERT_TRUE(is_lan_connected);
  connection_manager.SetLanConnected(false);
  EXPECT_FALSE(connection_manager.IsLanConnected());
}

TEST(FakeConnectivityManager, TestGetCurrentConnection) {
  FakeConnectivityManager connection_manager;
  ConnectivityManager::ConnectionType connection =
      connection_manager.GetConnectionType();
  EXPECT_EQ(connection, ConnectivityManager::ConnectionType::kWifi);
  connection_manager.SetConnectionType(
      ConnectivityManager::ConnectionType::kEthernet);
  EXPECT_EQ(connection_manager.GetConnectionType(),
            ConnectivityManager::ConnectionType::kEthernet);
}

TEST(FakeConnectivityManager, TestListener) {
  FakeConnectivityManager connection_manager;
  ConnectivityManager::ConnectionType connection_type =
      connection_manager.GetConnectionType();
  bool is_lan_connected = false;
  connection_manager.RegisterConnectionListener(
      "test",
      [&connection_type, &is_lan_connected](
          ConnectivityManager::ConnectionType connection, bool connected) {
        connection_type = connection;
        is_lan_connected = connected;
      });
  EXPECT_EQ(connection_manager.GetListenerCount(), 1);
  connection_manager.SetConnectionType(
      ConnectivityManager::ConnectionType::kEthernet);
  connection_manager.SetLanConnected(true);
  EXPECT_EQ(connection_type, ConnectivityManager::ConnectionType::kEthernet);
  ASSERT_TRUE(is_lan_connected);
  connection_manager.UnregisterConnectionListener("test");
  EXPECT_EQ(connection_manager.GetListenerCount(), 0);
  connection_manager.SetConnectionType(
      ConnectivityManager::ConnectionType::kWifi);
  EXPECT_EQ(connection_type, ConnectivityManager::ConnectionType::kEthernet);
  EXPECT_TRUE(is_lan_connected);
}

}  // namespace
}  // namespace nearby
