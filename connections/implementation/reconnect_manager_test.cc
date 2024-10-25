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

#include "connections/implementation/reconnect_manager.h"

#include <memory>
#include <string>
#include <tuple>

#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/endpoint_channel_manager.h"
#include "connections/implementation/mediums/mediums.h"
#include "connections/implementation/simulation_user.h"
#include "connections/medium_selector.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/feature_flags.h"
#include "internal/platform/logging.h"
#include "internal/platform/medium_environment.h"

namespace nearby {
namespace connections {
namespace {

constexpr absl::string_view kServiceId = "service-id";
constexpr absl::string_view kDeviceA = "device-a";
constexpr absl::string_view kDeviceB = "device-b";
constexpr absl::Duration kDefaultTimeout = absl::Milliseconds(1000);

constexpr BooleanMediumSelector kTestCases[] = {
  BooleanMediumSelector{
    .bluetooth = true
  },
};

class ReconnectSimulatorUser : public SimulationUser {
 public:
  explicit ReconnectSimulatorUser(
      absl::string_view name,
      BooleanMediumSelector allowed = BooleanMediumSelector())
      : SimulationUser(std::string(name), allowed,
                       SetSafeToDisconnect(true, true, false, 3)) {}
  ~ReconnectSimulatorUser() override {
    NEARBY_LOGS(INFO) << "ReconnectSimulatorUser: [down] name=" << info_.data();
  }

  bool IsConnected() const {
    return client_.IsConnectedToEndpoint(discovered_.endpoint_id);
  }

 protected:
};

class ReconnectManagerTest
    : public ::testing::TestWithParam<std::tuple<BooleanMediumSelector, bool>> {
 protected:
  bool SetupConnection(ReconnectSimulatorUser& user_a,
                       ReconnectSimulatorUser& user_b) {
    user_a.StartAdvertising(std::string(kServiceId), &connection_latch_);
    user_b.StartDiscovery(std::string(kServiceId), &discovery_latch_);
    EXPECT_TRUE(discovery_latch_.Await(kDefaultTimeout).result());
    EXPECT_EQ(user_b.GetDiscovered().service_id, kServiceId);
    EXPECT_EQ(user_b.GetDiscovered().endpoint_info, user_a.GetInfo());
    EXPECT_FALSE(user_b.GetDiscovered().endpoint_id.empty());
    NEARBY_LOGS(INFO) << "EP-B: [discovered]"
                      << user_b.GetDiscovered().endpoint_id;
    user_b.RequestConnection(&connection_latch_);
    EXPECT_TRUE(connection_latch_.Await(kDefaultTimeout).result());
    EXPECT_FALSE(user_a.GetDiscovered().endpoint_id.empty());
    NEARBY_LOGS(INFO) << "EP-A: [discovered]"
                      << user_a.GetDiscovered().endpoint_id;
    NEARBY_LOGS(INFO) << "Both users discovered their peers.";
    user_a.AcceptConnection(&accept_latch_);
    user_b.AcceptConnection(&accept_latch_);
    EXPECT_TRUE(accept_latch_.Await(kDefaultTimeout).result());
    NEARBY_LOGS(INFO) << "Both users reached connected state.";
    return user_a.IsConnected() && user_b.IsConnected();
  }

  CountDownLatch discovery_latch_{1};
  CountDownLatch connection_latch_{2};
  CountDownLatch accept_latch_{2};
  CountDownLatch reject_latch_{1};
  MediumEnvironment& env_{MediumEnvironment::Instance()};
};

TEST_P(ReconnectManagerTest, AllowReconnect) {
  FeatureFlags::Flags feature_flags = {
      .enable_cancellation_flag = std::get<1>(GetParam())};
  env_.SetFeatureFlags(feature_flags);
  env_.Start();

  ReconnectSimulatorUser user_a(kDeviceA, std::get<0>(GetParam()));
  ReconnectSimulatorUser user_b(kDeviceB, std::get<0>(GetParam()));
  ASSERT_TRUE(SetupConnection(user_a, user_b));

  Mediums mediums;
  ReconnectManager::AutoReconnectCallback auto_reconnect_callback = {
      .on_reconnect_success_cb =
          [&](ClientProxy* client, const std::string& endpoint_id) {
            NEARBY_LOGS(INFO)
                << " Reconnect successfully for endpoint_id: " << endpoint_id;
          },
      .on_reconnect_failure_cb =
          [&](ClientProxy* client, const std::string& endpoint_id,
              bool send_disconnection_notification,
              DisconnectionReason disconnection_reason) {
            NEARBY_LOGS(INFO)
                << " Reconnect failed for endpoint_id: " << endpoint_id;
          },
  };

  auto& client_a = user_a.GetClient();
  auto& client_b = user_b.GetClient();
  EndpointChannelManager& ecm_a = user_a.GetEndpointChannelManager();
  EndpointChannelManager& ecm_b = user_b.GetEndpointChannelManager();

  auto reconnect_manager_a = std::make_unique<ReconnectManager>(mediums, ecm_a);
  auto reconnect_manager_b = std::make_unique<ReconnectManager>(mediums, ecm_b);
  EXPECT_TRUE(reconnect_manager_a->AutoReconnect(
      &client_a, user_a.GetDiscovered().endpoint_id, auto_reconnect_callback,
      /*send_disconnection_notification=*/false,
      DisconnectionReason::UNFINISHED));
  EXPECT_TRUE(reconnect_manager_b->AutoReconnect(
      &client_b, user_b.GetDiscovered().endpoint_id, auto_reconnect_callback,
      /*send_disconnection_notification=*/false,
      DisconnectionReason::UNFINISHED));

  NEARBY_LOGS(INFO) << "Test completed.";
  user_a.Stop();
  user_b.Stop();
  env_.Stop();
}

INSTANTIATE_TEST_SUITE_P(ParametrisedReconnectManagerTest, ReconnectManagerTest,
                         ::testing::Combine(::testing::ValuesIn(kTestCases),
                                            ::testing::Bool()));

// More test will be added later.

}  // namespace
}  // namespace connections
}  // namespace nearby
