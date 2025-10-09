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

#include "internal/platform/implementation/windows/network_info.h"

#include "gtest/gtest.h"
#include "internal/platform/logging.h"

namespace nearby::windows {
namespace {

TEST(NetworkInfoTest, Refresh) {
  NetworkInfo network_info;
  EXPECT_TRUE(network_info.Refresh());
  EXPECT_FALSE(network_info.GetInterfaces().empty());
}

TEST(NetworkInfoTest, RenewIpv4Address) {
  NetworkInfo network_info;
  EXPECT_TRUE(network_info.Refresh());
  for (const auto& net_interface : network_info.GetInterfaces()) {
    if (net_interface.ipv4_addresses.empty()) {
      LOG(INFO) << "Ipv6 only interface";
    }
    EXPECT_TRUE(network_info.RenewIpv4Address(net_interface.luid));
  }
}

}  // namespace
}  // namespace nearby::windows
