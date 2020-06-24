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

#include "core_v2/internal/p2p_cluster_pcp_handler.h"

#include <memory>

#include "core_v2/options.h"
#include "platform_v2/base/medium_environment.h"
#include "platform_v2/public/count_down_latch.h"
#include "platform_v2/public/logging.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/time/time.h"

namespace location {
namespace nearby {
namespace connections {
namespace {

class P2pClusterPcpHandlerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    NEARBY_LOG(INFO, "SetUp: begin");
    env_.Stop();
    NEARBY_LOG(INFO, "SetUp: end");
  }

  ClientProxy client_a_;
  ClientProxy client_b_;
  std::string service_id_{"service"};
  ConnectionOptions options_{.strategy = Strategy::kP2pCluster};
  MediumEnvironment& env_{MediumEnvironment::Instance()};
};

TEST_F(P2pClusterPcpHandlerTest, CanConstructOne) {
  env_.Start();
  Mediums mediums;
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  P2pClusterPcpHandler handler(mediums, &em, &ecm);
  env_.Stop();
}

TEST_F(P2pClusterPcpHandlerTest, CanConstructMultiple) {
  env_.Start();
  Mediums mediums_a;
  Mediums mediums_b;
  EndpointChannelManager ecm_a;
  EndpointChannelManager ecm_b;
  EndpointManager em_a(&ecm_a);
  EndpointManager em_b(&ecm_b);
  P2pClusterPcpHandler handler_a(mediums_a, &em_a, &ecm_a);
  P2pClusterPcpHandler handler_b(mediums_b, &em_b, &ecm_b);
  env_.Stop();
}

TEST_F(P2pClusterPcpHandlerTest, CanAdvertise) {
  env_.Start();
  std::string endpoint_name{"endpoint_name"};
  Mediums mediums_a;
  EndpointChannelManager ecm_a;
  EndpointManager em_a(&ecm_a);
  P2pClusterPcpHandler handler_a(mediums_a, &em_a, &ecm_a);
  EXPECT_EQ(handler_a.StartAdvertising(&client_a_, service_id_, options_,
                                       {.name = endpoint_name}),
            Status{Status::kSuccess});
  env_.Stop();
}

TEST_F(P2pClusterPcpHandlerTest, CanDiscover) {
  env_.Start();
  std::string endpoint_name{"endpoint_name"};
  Mediums mediums_a;
  Mediums mediums_b;
  EndpointChannelManager ecm_a;
  EndpointChannelManager ecm_b;
  EndpointManager em_a(&ecm_a);
  EndpointManager em_b(&ecm_b);
  P2pClusterPcpHandler handler_a(mediums_a, &em_a, &ecm_a);
  P2pClusterPcpHandler handler_b(mediums_b, &em_b, &ecm_b);
  CountDownLatch latch(1);
  EXPECT_EQ(handler_a.StartAdvertising(&client_a_, service_id_, options_,
                                       {.name = endpoint_name}),
            Status{Status::kSuccess});
  EXPECT_EQ(handler_b.StartDiscovery(
                &client_b_, service_id_, options_,
                {
                    .endpoint_found_cb =
                        [&latch](const std::string& endpoint_id,
                                 const std::string& endpoint_name,
                                 const std::string& service_id) {
                          NEARBY_LOG(INFO, "Device discovered: id=%s",
                                     endpoint_id.c_str());
                          latch.CountDown();
                        },
                }),
            Status{Status::kSuccess});
  EXPECT_TRUE(latch.Await(absl::Milliseconds(1000)).result());
  env_.Stop();
}

TEST_F(P2pClusterPcpHandlerTest, CanConnect) {
  env_.Start();
  std::string endpoint_name_a{"endpoint_name"};
  Mediums mediums_a;
  Mediums mediums_b;
  BluetoothRadio& radio_a = mediums_a.GetBluetoothRadio();
  BluetoothRadio& radio_b = mediums_b.GetBluetoothRadio();
  radio_a.GetBluetoothAdapter().SetName("BT Device A");
  radio_b.GetBluetoothAdapter().SetName("BT Device B");
  EndpointChannelManager ecm_a;
  EndpointChannelManager ecm_b;
  EndpointManager em_a(&ecm_a);
  EndpointManager em_b(&ecm_b);
  P2pClusterPcpHandler handler_a(mediums_a, &em_a, &ecm_a);
  P2pClusterPcpHandler handler_b(mediums_b, &em_b, &ecm_b);
  CountDownLatch discover_latch(1);
  CountDownLatch connect_latch(2);
  struct DiscoveredInfo {
    std::string endpoint_id;
    std::string endpoint_name;
    std::string service_id;
  } discovered;
  EXPECT_EQ(
      handler_a.StartAdvertising(
          &client_a_, service_id_, options_,
          {
              .name = endpoint_name_a,
              .listener =
                  {
                      .initiated_cb =
                          [&connect_latch](const std::string& endpoint_id,
                                           const ConnectionResponseInfo& info) {
                            NEARBY_LOG(INFO,
                                       "StartAdvertising: initiated_cb called");
                            connect_latch.CountDown();
                          },
                  },
          }),
      Status{Status::kSuccess});
  EXPECT_EQ(handler_b.StartDiscovery(
                &client_b_, service_id_, options_,
                {
                    .endpoint_found_cb =
                        [&discover_latch, &discovered](
                            const std::string& endpoint_id,
                            const std::string& endpoint_name,
                            const std::string& service_id) {
                          NEARBY_LOG(INFO, "Device discovered: id=%s",
                                     endpoint_id.c_str());
                          discovered = {
                              .endpoint_id = endpoint_id,
                              .endpoint_name = endpoint_name,
                              .service_id = service_id,
                          };
                          discover_latch.CountDown();
                        },
                }),
            Status{Status::kSuccess});

  EXPECT_TRUE(discover_latch.Await(absl::Milliseconds(1000)).result());
  EXPECT_EQ(endpoint_name_a, discovered.endpoint_name);

  handler_b.RequestConnection(
      &client_b_, discovered.endpoint_id,
      {
          .name = discovered.endpoint_name,
          .listener =
              {
                  .initiated_cb =
                      [&connect_latch](const std::string& endpoint_id,
                                       const ConnectionResponseInfo& info) {
                        NEARBY_LOG(INFO,
                                   "RequestConnection: initiated_cb called");
                        connect_latch.CountDown();
                      },
              },
      });
  EXPECT_TRUE(connect_latch.Await(absl::Milliseconds(1000)).result());
  env_.Stop();
}

}  // namespace
}  // namespace connections
}  // namespace nearby
}  // namespace location
