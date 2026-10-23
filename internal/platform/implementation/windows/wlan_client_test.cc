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

#include "internal/platform/implementation/windows/wlan_client.h"

#include <vector>

#include "gtest/gtest.h"

namespace nearby::windows {

class WlanClientTest : public ::testing::Test {
 protected:
  std::vector<WlanClient::InterfaceInfo> PopulateInterfaceInfos(
      PWLAN_INTERFACE_INFO_LIST interfaces) const {
    return wlan_client_.PopulateInterfaceInfos(interfaces);
  }

  WlanClient wlan_client_;
};

TEST_F(WlanClientTest, Initialize) {
  // Forge machines don't have wifi.
  EXPECT_FALSE(wlan_client_.Initialize());
  EXPECT_EQ(wlan_client_.GetHandle(), nullptr);
}

TEST_F(WlanClientTest, PopulateInterfaceInfos) {
  int num_of_items = 3;
  PWLAN_INTERFACE_INFO_LIST interfaces =
      reinterpret_cast<PWLAN_INTERFACE_INFO_LIST>(
          ::WlanAllocateMemory(sizeof(WLAN_INTERFACE_INFO_LIST) +
                               num_of_items * sizeof(WLAN_INTERFACE_INFO)));
  interfaces->dwNumberOfItems = num_of_items;
  interfaces->InterfaceInfo[0].isState = wlan_interface_state_connected;
  interfaces->InterfaceInfo[1].isState = wlan_interface_state_not_ready;
  interfaces->InterfaceInfo[2].isState = wlan_interface_state_disconnected;
  std::vector<WlanClient::InterfaceInfo> interface_infos =
      PopulateInterfaceInfos(interfaces);
  EXPECT_EQ(interface_infos.size(), 3);
  EXPECT_TRUE(interface_infos[0].connected);
  EXPECT_TRUE(interface_infos[0].ready);
  EXPECT_FALSE(interface_infos[1].connected);
  EXPECT_FALSE(interface_infos[1].ready);
  EXPECT_FALSE(interface_infos[2].connected);
  EXPECT_TRUE(interface_infos[2].ready);
  ::WlanFreeMemory(interfaces);
}

}  // namespace nearby::windows
