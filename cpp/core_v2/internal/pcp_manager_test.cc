// Copyright 2020 Google LLC
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

#include "core_v2/internal/pcp_manager.h"

#include <string>

#include "core_v2/internal/endpoint_channel_manager.h"
#include "core_v2/internal/simulation_user.h"
#include "core_v2/options.h"
#include "platform_v2/base/medium_environment.h"
#include "platform_v2/public/count_down_latch.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/time/time.h"

namespace location {
namespace nearby {
namespace connections {
namespace {

constexpr char kServiceId[] = "service-id";
constexpr char kDeviceA[] = "device-A";
constexpr char kDeviceB[] = "device-B";

constexpr BooleanMediumSelector kTestCases[] = {
    BooleanMediumSelector{
        .bluetooth = true,
    },
    BooleanMediumSelector{
        .wifi_lan = true,
    },
    BooleanMediumSelector{
        .bluetooth = true,
        .wifi_lan = true,
    },
};

class PcpManagerTest : public ::testing::TestWithParam<BooleanMediumSelector> {
 protected:
  PcpManagerTest() { env_.Stop(); }

  MediumEnvironment& env_{MediumEnvironment::Instance()};
};

TEST_P(PcpManagerTest, CanCreateOne) {
  env_.Start();
  SimulationUser user(kDeviceA, GetParam());
  env_.Stop();
}

TEST_P(PcpManagerTest, CanCreateMany) {
  env_.Start();
  SimulationUser user_a(kDeviceA, GetParam());
  SimulationUser user_b(kDeviceB, GetParam());
  env_.Stop();
}

TEST_P(PcpManagerTest, CanAdvertise) {
  env_.Start();
  SimulationUser user_a(kDeviceA, GetParam());
  SimulationUser user_b(kDeviceB, GetParam());
  user_a.StartAdvertising(kServiceId, nullptr);
  env_.Stop();
}

TEST_P(PcpManagerTest, CanDiscover) {
  env_.Start();
  SimulationUser user_a("device-a", GetParam());
  SimulationUser user_b("device-b", GetParam());
  user_a.StartAdvertising(kServiceId, nullptr);
  CountDownLatch latch(1);
  user_b.StartDiscovery(kServiceId, &latch);
  EXPECT_TRUE(latch.Await(absl::Milliseconds(1000)).result());
  EXPECT_EQ(user_b.GetDiscovered().service_id, kServiceId);
  EXPECT_EQ(user_b.GetDiscovered().endpoint_info, user_a.GetInfo());
  env_.Stop();
}

TEST_P(PcpManagerTest, CanConnect) {
  env_.Start();
  SimulationUser user_a("device-a", GetParam());
  SimulationUser user_b("device-b", GetParam());
  CountDownLatch discovery_latch(1);
  CountDownLatch connection_latch(2);
  user_a.StartAdvertising(kServiceId, &connection_latch);
  user_b.StartDiscovery(kServiceId, &discovery_latch);
  EXPECT_TRUE(discovery_latch.Await(absl::Milliseconds(1000)).result());
  EXPECT_EQ(user_b.GetDiscovered().service_id, kServiceId);
  EXPECT_EQ(user_b.GetDiscovered().endpoint_info, user_a.GetInfo());
  user_b.RequestConnection(&connection_latch);
  EXPECT_TRUE(connection_latch.Await(absl::Milliseconds(1000)).result());
  user_a.Stop();
  user_b.Stop();
  env_.Stop();
}

TEST_P(PcpManagerTest, CanAccept) {
  env_.Start();
  SimulationUser user_a("device-a", GetParam());
  SimulationUser user_b("device-b", GetParam());
  CountDownLatch discovery_latch(1);
  CountDownLatch connection_latch(2);
  CountDownLatch accept_latch(2);
  user_a.StartAdvertising(kServiceId, &connection_latch);
  user_b.StartDiscovery(kServiceId, &discovery_latch);
  EXPECT_TRUE(discovery_latch.Await(absl::Milliseconds(1000)).result());
  EXPECT_EQ(user_b.GetDiscovered().service_id, kServiceId);
  EXPECT_EQ(user_b.GetDiscovered().endpoint_info, user_a.GetInfo());
  user_b.RequestConnection(&connection_latch);
  EXPECT_TRUE(connection_latch.Await(absl::Milliseconds(1000)).result());
  user_a.AcceptConnection(&accept_latch);
  user_b.AcceptConnection(&accept_latch);
  EXPECT_TRUE(accept_latch.Await(absl::Milliseconds(1000)).result());
  user_a.Stop();
  user_b.Stop();
  env_.Stop();
}

TEST_P(PcpManagerTest, CanReject) {
  env_.Start();
  SimulationUser user_a("device-a", GetParam());
  SimulationUser user_b("device-b", GetParam());
  CountDownLatch discovery_latch(1);
  CountDownLatch connection_latch(2);
  CountDownLatch reject_latch(1);
  user_a.StartAdvertising(kServiceId, &connection_latch);
  user_b.StartDiscovery(kServiceId, &discovery_latch);
  EXPECT_TRUE(discovery_latch.Await(absl::Milliseconds(1000)).result());
  EXPECT_EQ(user_b.GetDiscovered().service_id, kServiceId);
  EXPECT_EQ(user_b.GetDiscovered().endpoint_info, user_a.GetInfo());
  user_b.RequestConnection(&connection_latch);
  EXPECT_TRUE(connection_latch.Await(absl::Milliseconds(1000)).result());
  user_b.ExpectRejectedConnection(reject_latch);
  user_a.RejectConnection(nullptr);
  EXPECT_TRUE(reject_latch.Await(absl::Milliseconds(1000)).result());
  user_a.Stop();
  user_b.Stop();
  env_.Stop();
}

INSTANTIATE_TEST_SUITE_P(ParametrisedPcpManagerTest, PcpManagerTest,
                         ::testing::ValuesIn(kTestCases));

}  // namespace
}  // namespace connections
}  // namespace nearby
}  // namespace location
