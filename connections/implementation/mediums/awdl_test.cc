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

#include "connections/implementation/mediums/awdl.h"

#include <string>
#include <utility>

#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "internal/platform/awdl.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/expected.h"
#include "internal/platform/feature_flags.h"
#include "internal/platform/implementation/psk_info.h"
#include "internal/platform/logging.h"
#include "internal/platform/medium_environment.h"
#include "internal/platform/nsd_service_info.h"

namespace nearby {
namespace connections {
namespace {

using FeatureFlags = FeatureFlags::Flags;

constexpr FeatureFlags kTestCases[] = {
    FeatureFlags{
        .enable_cancellation_flag = true,
    },
    FeatureFlags{
        .enable_cancellation_flag = false,
    },
};

constexpr absl::Duration kWaitDuration = absl::Milliseconds(1000);
constexpr absl::string_view kServiceID{"com.google.location.nearby.apps.test"};
constexpr absl::string_view kServiceInfoName{"ServiceInfoName"};
constexpr absl::string_view kEndpointName{"EndpointName"};
constexpr absl::string_view kEndpointInfoKey{"n"};

class AwdlTest : public ::testing::TestWithParam<FeatureFlags> {
 protected:
  using DiscoveredServiceCallback = AwdlMedium::DiscoveredServiceCallback;

  MediumEnvironment& env_{MediumEnvironment::Instance()};
};

TEST_P(AwdlTest, CanConnect) {
  FeatureFlags feature_flags = GetParam();
  env_.SetFeatureFlags(feature_flags);
  env_.Start();
  Awdl awdl_client;
  Awdl awdl_server;
  std::string service_id(kServiceID);
  std::string service_info_name(kServiceInfoName);
  std::string endpoint_info_name(kEndpointName);
  CountDownLatch discovered_latch(1);
  CountDownLatch accept_latch(1);

  AwdlSocket socket_for_server;
  EXPECT_TRUE(awdl_server.StartAcceptingConnections(
      service_id, [&](const std::string& service_id, AwdlSocket socket) {
        socket_for_server = std::move(socket);
        accept_latch.CountDown();
      }));

  NsdServiceInfo nsd_service_info;
  nsd_service_info.SetServiceName(service_info_name);
  nsd_service_info.SetTxtRecord(std::string(kEndpointInfoKey),
                                endpoint_info_name);
  awdl_server.StartAdvertising(service_id, nsd_service_info);

  NsdServiceInfo discovered_service_info;
  awdl_client.StartDiscovery(
      service_id,
      {
          .service_discovered_cb =
              [&discovered_latch, &discovered_service_info](
                  NsdServiceInfo service_info, const std::string& service_id) {
                LOG(INFO) << "Discovered service_info=" << &service_info;
                discovered_service_info = service_info;
                discovered_latch.CountDown();
              },
      });
  discovered_latch.Await(kWaitDuration).result();
  ASSERT_TRUE(discovered_service_info.IsValid());

  CancellationFlag flag;
  ErrorOr<AwdlSocket> socket_for_client_result =
      awdl_client.Connect(service_id, discovered_service_info, &flag);
  EXPECT_TRUE(accept_latch.Await(kWaitDuration).result());
  EXPECT_TRUE(awdl_server.StopAcceptingConnections(service_id));
  EXPECT_TRUE(awdl_server.StopAdvertising(service_id));
  EXPECT_TRUE(socket_for_server.IsValid());
  EXPECT_TRUE(socket_for_client_result.has_value());
  EXPECT_TRUE(socket_for_client_result.value().IsValid());
  env_.Stop();
}

TEST_P(AwdlTest, CanConnectWithPsk) {
  FeatureFlags feature_flags = GetParam();
  env_.SetFeatureFlags(feature_flags);
  env_.Start();
  Awdl awdl_client;
  Awdl awdl_server;
  std::string service_id(kServiceID);
  std::string service_info_name(kServiceInfoName);
  std::string endpoint_info_name(kEndpointName);
  api::PskInfo psk_info;
  psk_info.password = "password";
  CountDownLatch discovered_latch(1);
  CountDownLatch accept_latch(1);

  AwdlSocket socket_for_server;
  EXPECT_TRUE(awdl_server.StartAcceptingConnections(
      service_id, psk_info,
      [&](const std::string& service_id, AwdlSocket socket) {
        socket_for_server = std::move(socket);
        accept_latch.CountDown();
      }));

  NsdServiceInfo nsd_service_info;
  nsd_service_info.SetServiceName(service_info_name);
  nsd_service_info.SetTxtRecord(std::string(kEndpointInfoKey),
                                endpoint_info_name);
  awdl_server.StartAdvertising(service_id, nsd_service_info);

  NsdServiceInfo discovered_service_info;
  awdl_client.StartDiscovery(
      service_id,
      {
          .service_discovered_cb =
              [&discovered_latch, &discovered_service_info](
                  NsdServiceInfo service_info, const std::string& service_id) {
                LOG(INFO) << "Discovered service_info=" << &service_info;
                discovered_service_info = service_info;
                discovered_latch.CountDown();
              },
      });
  discovered_latch.Await(kWaitDuration).result();
  ASSERT_TRUE(discovered_service_info.IsValid());

  CancellationFlag flag;
  ErrorOr<AwdlSocket> socket_for_client_result =
      awdl_client.Connect(service_id, discovered_service_info, psk_info, &flag);
  EXPECT_TRUE(accept_latch.Await(kWaitDuration).result());
  EXPECT_TRUE(awdl_server.StopAcceptingConnections(service_id));
  EXPECT_TRUE(awdl_server.StopAdvertising(service_id));
  EXPECT_TRUE(socket_for_server.IsValid());
  EXPECT_TRUE(socket_for_client_result.has_value());
  EXPECT_TRUE(socket_for_client_result.value().IsValid());
  env_.Stop();
}

TEST_P(AwdlTest, CanCancelConnect) {
  FeatureFlags feature_flags = GetParam();
  env_.SetFeatureFlags(feature_flags);
  env_.Start();
  Awdl awdl_client;
  Awdl awdl_server;
  std::string service_id(kServiceID);
  std::string service_info_name(kServiceInfoName);
  std::string endpoint_info_name(kEndpointName);
  CountDownLatch discovered_latch(1);
  CountDownLatch accept_latch(1);

  AwdlSocket socket_for_server;
  EXPECT_TRUE(awdl_server.StartAcceptingConnections(
      service_id, [&](const std::string& service_id, AwdlSocket socket) {
        socket_for_server = std::move(socket);
        accept_latch.CountDown();
      }));

  NsdServiceInfo nsd_service_info;
  nsd_service_info.SetServiceName(service_info_name);
  nsd_service_info.SetTxtRecord(std::string(kEndpointInfoKey),
                                endpoint_info_name);
  awdl_server.StartAdvertising(service_id, nsd_service_info);

  NsdServiceInfo discovered_service_info;
  awdl_client.StartDiscovery(
      service_id,
      {
          .service_discovered_cb =
              [&discovered_latch, &discovered_service_info](
                  NsdServiceInfo service_info, const std::string& service_id) {
                LOG(INFO) << "Discovered service_info=" << &service_info;
                discovered_service_info = service_info;
                discovered_latch.CountDown();
              },
      });
  EXPECT_TRUE(discovered_latch.Await(kWaitDuration).result());
  ASSERT_TRUE(discovered_service_info.IsValid());

  CancellationFlag flag(true);
  ErrorOr<AwdlSocket> socket_for_client_result =
      awdl_client.Connect(service_id, discovered_service_info, &flag);
  // If FeatureFlag is disabled, Cancelled is false as no-op.
  if (!feature_flags.enable_cancellation_flag) {
    EXPECT_TRUE(accept_latch.Await(kWaitDuration).result());
    EXPECT_TRUE(awdl_server.StopAcceptingConnections(service_id));
    EXPECT_TRUE(awdl_server.StopAdvertising(service_id));
    EXPECT_TRUE(socket_for_server.IsValid());
    EXPECT_TRUE(socket_for_client_result.has_value());
    EXPECT_TRUE(socket_for_client_result.value().IsValid());
  } else {
    EXPECT_FALSE(accept_latch.Await(kWaitDuration).result());
    EXPECT_TRUE(awdl_server.StopAcceptingConnections(service_id));
    EXPECT_TRUE(awdl_server.StopAdvertising(service_id));
    EXPECT_FALSE(socket_for_server.IsValid());
    EXPECT_TRUE(socket_for_client_result.has_error());
  }
  env_.Stop();
}

INSTANTIATE_TEST_SUITE_P(ParametrisedAwdlTest, AwdlTest,
                         ::testing::ValuesIn(kTestCases));

TEST_F(AwdlTest, CanConstructValidObject) {
  env_.Start();
  Awdl awdl_a;
  Awdl awdl_b;
  std::string service_id(kServiceID);

  EXPECT_TRUE(awdl_a.IsAvailable());
  EXPECT_TRUE(awdl_b.IsAvailable());
  env_.Stop();
}

TEST_F(AwdlTest, CanStartAdvertising) {
  env_.Start();
  Awdl awdl_a;
  std::string service_id(kServiceID);
  std::string service_info_name(kServiceInfoName);
  std::string endpoint_info_name(kEndpointName);

  EXPECT_TRUE(awdl_a.StartAcceptingConnections(service_id, {}));

  NsdServiceInfo nsd_service_info;
  nsd_service_info.SetServiceName(service_info_name);
  nsd_service_info.SetTxtRecord(std::string(kEndpointInfoKey),
                                endpoint_info_name);
  EXPECT_TRUE(awdl_a.StartAdvertising(service_id, nsd_service_info));
  EXPECT_TRUE(awdl_a.StopAdvertising(service_id));
  env_.Stop();
}

TEST_F(AwdlTest, StartAdvertisingFailsWithInvalidNsdServiceInfo) {
  env_.Start();
  Awdl awdl_a;
  std::string service_id(kServiceID);

  EXPECT_TRUE(awdl_a.StartAcceptingConnections(service_id, {}));

  NsdServiceInfo nsd_service_info;
  ErrorOr<bool> result = awdl_a.StartAdvertising(service_id, nsd_service_info);
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().operation_result_code().value(),
            location::nearby::proto::connections::OperationResultCode::
                MEDIUM_UNAVAILABLE_NSD_NOT_AVAILABLE);
  env_.Stop();
}

TEST_F(AwdlTest, StopAdvertisingFailsIfNotAdvertising) {
  env_.Start();
  Awdl awdl_a;
  std::string service_id(kServiceID);

  EXPECT_FALSE(awdl_a.StopAdvertising(service_id));
  env_.Stop();
}

TEST_F(AwdlTest, StartAdvertisingFailsIfAlreadyAdvertising) {
  env_.Start();
  Awdl awdl_a;
  std::string service_id(kServiceID);
  std::string service_info_name(kServiceInfoName);
  std::string endpoint_info_name(kEndpointName);

  EXPECT_TRUE(awdl_a.StartAcceptingConnections(service_id, {}));

  NsdServiceInfo nsd_service_info;
  nsd_service_info.SetServiceName(service_info_name);
  nsd_service_info.SetTxtRecord(std::string(kEndpointInfoKey),
                                endpoint_info_name);
  EXPECT_TRUE(awdl_a.StartAdvertising(service_id, nsd_service_info));
  ErrorOr<bool> result = awdl_a.StartAdvertising(service_id, nsd_service_info);
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().operation_result_code().value(),
            location::nearby::proto::connections::OperationResultCode::
                CLIENT_AWDL_DUPLICATE_ADVERTISING);
  EXPECT_TRUE(awdl_a.StopAdvertising(service_id));
  env_.Stop();
}

TEST_F(AwdlTest, StartAdvertisingFailsIfNotAcceptingConnections) {
  env_.Start();
  Awdl awdl_a;
  std::string service_id(kServiceID);
  std::string service_info_name(kServiceInfoName);
  std::string endpoint_info_name(kEndpointName);

  NsdServiceInfo nsd_service_info;
  nsd_service_info.SetServiceName(service_info_name);
  nsd_service_info.SetTxtRecord(std::string(kEndpointInfoKey),
                                endpoint_info_name);
  ErrorOr<bool> result = awdl_a.StartAdvertising(service_id, nsd_service_info);
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().operation_result_code().value(),
            location::nearby::proto::connections::OperationResultCode::
                CLIENT_DUPLICATE_ACCEPTING_AWDL_CONNECTION_REQUEST);
  env_.Stop();
}

TEST_F(AwdlTest, StartAdvertisingUpdatesNsdServiceInfo) {
  env_.Start();
  Awdl awdl_a;
  std::string service_id(kServiceID);
  std::string service_info_name(kServiceInfoName);
  std::string endpoint_info_name(kEndpointName);

  EXPECT_TRUE(awdl_a.StartAcceptingConnections(service_id, {}));

  NsdServiceInfo nsd_service_info;
  nsd_service_info.SetServiceName(service_info_name);
  nsd_service_info.SetTxtRecord(std::string(kEndpointInfoKey),
                                endpoint_info_name);
  EXPECT_TRUE(awdl_a.StartAdvertising(service_id, nsd_service_info));
  EXPECT_FALSE(nsd_service_info.GetServiceType().empty());
  EXPECT_FALSE(nsd_service_info.GetIPAddress().empty());
  EXPECT_GT(nsd_service_info.GetPort(), 0);
  EXPECT_TRUE(awdl_a.StopAdvertising(service_id));
  env_.Stop();
}

TEST_F(AwdlTest, CanStartAcceptingConnectionsWithPsk) {
  env_.Start();
  Awdl awdl_a;
  std::string service_id(kServiceID);
  std::string service_info_name(kServiceInfoName);
  std::string endpoint_info_name(kEndpointName);
  api::PskInfo psk_info;
  psk_info.password = "password";

  EXPECT_TRUE(awdl_a.StartAcceptingConnections(service_id, psk_info, {}));

  NsdServiceInfo nsd_service_info;
  nsd_service_info.SetServiceName(service_info_name);
  nsd_service_info.SetTxtRecord(std::string(kEndpointInfoKey),
                                endpoint_info_name);
  EXPECT_TRUE(awdl_a.StartAdvertising(service_id, nsd_service_info));
  EXPECT_EQ(awdl_a.GetCredentials(service_id).password, "password");
  EXPECT_TRUE(awdl_a.StopAdvertising(service_id));
  env_.Stop();
}

TEST_F(AwdlTest, CanStartMultipleAdvertising) {
  env_.Start();
  Awdl awdl_a;
  std::string service_id_1(kServiceID);
  std::string service_id_2("com.google.location.nearby.apps.test_1");
  std::string service_info_name_1(kServiceInfoName);
  std::string service_info_name_2("ServiceInfoName_1");
  std::string endpoint_info_name(kEndpointName);

  EXPECT_TRUE(awdl_a.StartAcceptingConnections(service_id_1, {}));
  EXPECT_TRUE(awdl_a.StartAcceptingConnections(service_id_2, {}));

  NsdServiceInfo nsd_service_info_1;
  nsd_service_info_1.SetServiceName(service_info_name_1);
  nsd_service_info_1.SetTxtRecord(std::string(kEndpointInfoKey),
                                  endpoint_info_name);
  NsdServiceInfo nsd_service_info_2;
  nsd_service_info_2.SetServiceName(service_info_name_2);
  nsd_service_info_2.SetTxtRecord(std::string(kEndpointInfoKey),
                                  endpoint_info_name);
  EXPECT_TRUE(awdl_a.StartAdvertising(service_id_1, nsd_service_info_1));
  EXPECT_TRUE(awdl_a.StartAdvertising(service_id_2, nsd_service_info_2));
  EXPECT_TRUE(awdl_a.StopAdvertising(service_id_1));
  EXPECT_TRUE(awdl_a.StopAdvertising(service_id_2));
  EXPECT_TRUE(awdl_a.StopAcceptingConnections(service_id_1));
  EXPECT_TRUE(awdl_a.StopAcceptingConnections(service_id_2));
  env_.Stop();
}

TEST_F(AwdlTest, StartAcceptingConnectionsFailsWithEmptyServiceId) {
  env_.Start();
  Awdl awdl_a;
  ErrorOr<bool> result = awdl_a.StartAcceptingConnections("", {});
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().operation_result_code().value(),
            location::nearby::proto::connections::OperationResultCode::
                NEARBY_LOCAL_CLIENT_STATE_WRONG);
  env_.Stop();
}

TEST_F(AwdlTest, StartAcceptingConnectionsFailsIfAlreadyAccepting) {
  env_.Start();
  Awdl awdl_a;
  std::string service_id(kServiceID);

  EXPECT_TRUE(awdl_a.StartAcceptingConnections(service_id, {}));
  ErrorOr<bool> result = awdl_a.StartAcceptingConnections(service_id, {});
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error().operation_result_code().value(),
            location::nearby::proto::connections::OperationResultCode::
                CLIENT_DUPLICATE_ACCEPTING_AWDL_CONNECTION_REQUEST);
  awdl_a.StopAcceptingConnections(service_id);
  env_.Stop();
}

TEST_F(AwdlTest, CanStartDiscovery) {
  env_.Start();
  Awdl awdl_a;
  std::string service_id(kServiceID);

  EXPECT_TRUE(awdl_a.StartDiscovery(service_id, DiscoveredServiceCallback{}));
  EXPECT_TRUE(awdl_a.StopDiscovery(service_id));
  env_.Stop();
}

TEST_F(AwdlTest, CanStartMultipleDiscovery) {
  env_.Start();
  Awdl awdl_a;
  std::string service_id_1(kServiceID);
  std::string service_id_2("com.google.location.nearby.apps.test_1");

  EXPECT_TRUE(awdl_a.StartDiscovery(service_id_1, DiscoveredServiceCallback{}));

  EXPECT_TRUE(awdl_a.StartDiscovery(service_id_2, DiscoveredServiceCallback{}));
  EXPECT_TRUE(awdl_a.StopDiscovery(service_id_1));
  EXPECT_TRUE(awdl_a.StopDiscovery(service_id_2));
  env_.Stop();
}

TEST_F(AwdlTest, CanAdvertiseThatOtherMediumDiscover) {
  env_.Start();
  Awdl awdl_a;
  Awdl awdl_b;
  std::string service_id(kServiceID);
  std::string service_info_name(kServiceInfoName);
  std::string endpoint_info_name(kEndpointName);
  CountDownLatch discovered_latch(1);
  CountDownLatch lost_latch(1);

  awdl_b.StartDiscovery(
      service_id, DiscoveredServiceCallback{
                      .service_discovered_cb =
                          [&discovered_latch](NsdServiceInfo service_info,
                                              const std::string& service_id) {
                            discovered_latch.CountDown();
                          },
                      .service_lost_cb =
                          [&lost_latch](NsdServiceInfo service_info,
                                        const std::string& service_id) {
                            lost_latch.CountDown();
                          },
                  });

  EXPECT_TRUE(awdl_a.StartAcceptingConnections(service_id, {}));

  NsdServiceInfo nsd_service_info;
  nsd_service_info.SetServiceName(service_info_name);
  nsd_service_info.SetTxtRecord(std::string(kEndpointInfoKey),
                                endpoint_info_name);
  EXPECT_TRUE(awdl_a.StartAdvertising(service_id, nsd_service_info));
  EXPECT_TRUE(discovered_latch.Await(kWaitDuration).result());
  EXPECT_TRUE(awdl_a.StopAdvertising(service_id));
  EXPECT_TRUE(lost_latch.Await(kWaitDuration).result());
  EXPECT_TRUE(awdl_b.StopDiscovery(service_id));
  env_.Stop();
}

TEST_F(AwdlTest, CanDiscoverThatOtherMediumAdvertise) {
  env_.Start();
  Awdl awdl_a;
  Awdl awdl_b;
  std::string service_id(kServiceID);
  std::string service_info_name(kServiceInfoName);
  std::string endpoint_info_name(kEndpointName);
  CountDownLatch discovered_latch(1);
  CountDownLatch lost_latch(1);

  EXPECT_TRUE(awdl_b.StartAcceptingConnections(service_id, {}));

  NsdServiceInfo nsd_service_info;
  nsd_service_info.SetServiceName(service_info_name);
  nsd_service_info.SetTxtRecord(std::string(kEndpointInfoKey),
                                endpoint_info_name);
  awdl_b.StartAdvertising(service_id, nsd_service_info);

  EXPECT_TRUE(awdl_a.StartDiscovery(
      service_id, DiscoveredServiceCallback{
                      .service_discovered_cb =
                          [&discovered_latch](NsdServiceInfo service_info,
                                              const std::string& service_id) {
                            discovered_latch.CountDown();
                          },
                      .service_lost_cb =
                          [&lost_latch](NsdServiceInfo service_info,
                                        const std::string& service_id) {
                            lost_latch.CountDown();
                          },
                  }));
  EXPECT_TRUE(discovered_latch.Await(kWaitDuration).result());
  EXPECT_TRUE(awdl_b.StopAdvertising(service_id));
  EXPECT_TRUE(lost_latch.Await(kWaitDuration).result());
  EXPECT_TRUE(awdl_a.StopDiscovery(service_id));
  env_.Stop();
}

}  // namespace
}  // namespace connections
}  // namespace nearby
