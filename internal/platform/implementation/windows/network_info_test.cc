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

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "internal/platform/logging.h"

namespace nearby::windows {

using ::testing::SizeIs;

class NetworkInfoTest : public ::testing::Test {
 protected:
  static void AddIpUnicastAddresses(
      IP_ADAPTER_UNICAST_ADDRESS* unicast_addresses,
      NetworkInfo::InterfaceInfo& net_interface) {
    NetworkInfo::AddIpUnicastAddresses(unicast_addresses, net_interface);
  }
};

namespace {

TEST_F(NetworkInfoTest, Refresh) {
  NetworkInfo network_info;
  EXPECT_TRUE(network_info.Refresh());
  EXPECT_FALSE(network_info.GetInterfaces().empty());
}

TEST_F(NetworkInfoTest, RenewIpv4Address) {
  NetworkInfo network_info;
  EXPECT_TRUE(network_info.Refresh());
  for (const auto& net_interface : network_info.GetInterfaces()) {
    if (net_interface.ipv4_addresses.empty()) {
      LOG(INFO) << "Ipv6 only interface";
    }
    EXPECT_TRUE(network_info.RenewIpv4Address(net_interface.luid));
  }
}

TEST_F(NetworkInfoTest, AddIpUnicastAddressesNoAddresss) {
  IP_ADAPTER_UNICAST_ADDRESS unicast_addresses;
  unicast_addresses.Address.lpSockaddr = nullptr;
  unicast_addresses.Next = nullptr;
  NetworkInfo::InterfaceInfo net_interface;
  AddIpUnicastAddresses(&unicast_addresses, net_interface);
  EXPECT_TRUE(net_interface.ipv4_addresses.empty());
  EXPECT_TRUE(net_interface.ipv6_addresses.empty());
}

TEST_F(NetworkInfoTest, AddIpUnicastAddresses) {
  IP_ADAPTER_UNICAST_ADDRESS unicast_addresses1;
  IP_ADAPTER_UNICAST_ADDRESS unicast_addresses2;
  unicast_addresses1.Next = &unicast_addresses2;
  unicast_addresses2.Next = nullptr;
  sockaddr_in ipv4_address = {
      .sin_family = AF_INET,
      .sin_addr = {{{0x01, 0x02, 0x03, 0x04}}},
  };
  unicast_addresses1.Address = {
      .lpSockaddr = reinterpret_cast<sockaddr*>(&ipv4_address),
      .iSockaddrLength = sizeof(ipv4_address),
  };;
  sockaddr_in6 ipv6_address = {
      .sin6_family = AF_INET6,
      .sin6_addr = {{{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
                      0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10}}},
  };
  unicast_addresses2.Address = {
      .lpSockaddr = reinterpret_cast<sockaddr*>(&ipv6_address),
      .iSockaddrLength = sizeof(ipv6_address),
  };
  NetworkInfo::InterfaceInfo net_interface;
  AddIpUnicastAddresses(&unicast_addresses1, net_interface);
  EXPECT_THAT(net_interface.ipv4_addresses, SizeIs(1));
  EXPECT_THAT(net_interface.ipv6_addresses, SizeIs(1));
  EXPECT_EQ(net_interface.ipv4_addresses[0].ToString(), "1.2.3.4");
  EXPECT_EQ(net_interface.ipv6_addresses[0].ToString(),
            "102:304:506:708:90a:b0c:d0e:f10");
}

}  // namespace
}  // namespace nearby::windows
