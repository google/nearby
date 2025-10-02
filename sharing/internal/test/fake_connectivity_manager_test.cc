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

#include "gtest/gtest.h"

namespace nearby {
namespace {

TEST(FakeConnectivityManager, TestIsLanConnected) {
  FakeConnectivityManager connection_manager;
  bool is_lan_connected = connection_manager.IsLanConnected();
  ASSERT_TRUE(is_lan_connected);
  connection_manager.SetLanConnected(false);
  EXPECT_FALSE(connection_manager.IsLanConnected());
}

TEST(FakeConnectivityManager, TestListener) {
  FakeConnectivityManager connection_manager;
  bool is_lan_connected = false;
  connection_manager.RegisterLanListener(
      "test", [&is_lan_connected](bool lan_connected) {
        is_lan_connected = lan_connected;
      });
  connection_manager.SetLanConnected(true);
  ASSERT_TRUE(is_lan_connected);
  connection_manager.UnregisterLanListener("test");
  EXPECT_TRUE(is_lan_connected);
}

}  // namespace
}  // namespace nearby
