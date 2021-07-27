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

#include "core/internal/client_proxy.h"

#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/container/flat_hash_set.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "core/listeners.h"
#include "core/options.h"
#include "core/strategy.h"
#include "platform/base/byte_array.h"
#include "platform/base/feature_flags.h"
#include "platform/base/medium_environment.h"

namespace location {
namespace nearby {
namespace connections {
namespace {

using FeatureFlags = FeatureFlags::Flags;
using ::testing::MockFunction;
using ::testing::StrictMock;

constexpr FeatureFlags kTestCases[] = {
    FeatureFlags{
        .enable_cancellation_flag = true,
    },
    FeatureFlags{
        .enable_cancellation_flag = false,
    },
};

class ClientProxyTest : public ::testing::TestWithParam<FeatureFlags> {
 protected:
  struct MockDiscoveryListener {
    StrictMock<MockFunction<void(const std::string& endpoint_id,
                                 const ByteArray& endpoint_info,
                                 const std::string& service_id)>>
        endpoint_found_cb;
    StrictMock<MockFunction<void(const std::string& endpoint_id)>>
        endpoint_lost_cb;
  };
  struct MockConnectionListener {
    StrictMock<MockFunction<void(const std::string& endpoint_id,
                                 const ConnectionResponseInfo& info)>>
        initiated_cb;
    StrictMock<MockFunction<void(const std::string& endpoint_id)>> accepted_cb;
    StrictMock<MockFunction<void(const std::string& endpoint_id,
                                 const Status& status)>>
        rejected_cb;
    StrictMock<MockFunction<void(const std::string& endpoint_id)>>
        disconnected_cb;
    StrictMock<MockFunction<void(const std::string& endpoint_id,
                                 std::int32_t quality)>>
        bandwidth_changed_cb;
  };
  struct MockPayloadListener {
    StrictMock<
        MockFunction<void(const std::string& endpoint_id, Payload payload)>>
        payload_cb;
    StrictMock<MockFunction<void(const std::string& endpoint_id,
                                 const PayloadProgressInfo& info)>>
        payload_progress_cb;
  };

  struct Endpoint {
    ByteArray info;
    std::string id;
  };

  bool ShouldEnterHighVisibilityMode(const ConnectionOptions& options) {
    return !options.low_power && options.allowed.bluetooth;
  }

  Endpoint StartAdvertising(
      ClientProxy* client, ConnectionListener listener,
      ConnectionOptions advertising_options = ConnectionOptions{}) {
    if (ShouldEnterHighVisibilityMode(advertising_options)) {
      client->EnterHighVisibilityMode();
    }
    Endpoint endpoint{
        .info = ByteArray{"advertising endpoint name"},
        .id = client->GetLocalEndpointId(),
    };
    client->StartedAdvertising(service_id_, strategy_, listener,
                               absl::MakeSpan(mediums_), advertising_options);
    return endpoint;
  }

  void StopAdvertising(ClientProxy* client) { client->StoppedAdvertising(); }

  Endpoint StartDiscovery(ClientProxy* client, DiscoveryListener listener) {
    Endpoint endpoint{
        .info = ByteArray{"discovery endpoint name"},
        .id = client->GetLocalEndpointId(),
    };
    client->StartedDiscovery(service_id_, strategy_, listener,
                             absl::MakeSpan(mediums_));
    return endpoint;
  }

  void OnDiscoveryEndpointFound(ClientProxy* client, const Endpoint& endpoint) {
    EXPECT_CALL(mock_discovery_.endpoint_found_cb, Call).Times(1);
    client->OnEndpointFound(service_id_, endpoint.id, endpoint.info, medium_);
  }

  void OnDiscoveryEndpointLost(ClientProxy* client, const Endpoint& endpoint) {
    EXPECT_CALL(mock_discovery_.endpoint_lost_cb, Call).Times(1);
    client->OnEndpointLost(service_id_, endpoint.id);
  }

  void OnDiscoveryConnectionInitiated(ClientProxy* client,
                                      const Endpoint& endpoint) {
    EXPECT_CALL(mock_discovery_connection_.initiated_cb, Call).Times(1);
    const std::string auth_token{"auth_token"};
    const ByteArray raw_auth_token{auth_token};
    advertising_connection_info_.remote_endpoint_info = endpoint.info;
    client->OnConnectionInitiated(endpoint.id, advertising_connection_info_,
                                  connection_options_,
                                  discovery_connection_listener_);
    EXPECT_TRUE(client->HasPendingConnectionToEndpoint(endpoint.id));
  }

  void OnDiscoveryConnectionLocalAccepted(ClientProxy* client,
                                          const Endpoint& endpoint) {
    EXPECT_TRUE(client->HasPendingConnectionToEndpoint(endpoint.id));
    EXPECT_FALSE(client->HasLocalEndpointResponded(endpoint.id));
    client->LocalEndpointAcceptedConnection(endpoint.id, payload_listener_);
    EXPECT_TRUE(client->HasLocalEndpointResponded(endpoint.id));
    EXPECT_TRUE(client->LocalConnectionIsAccepted(endpoint.id));
  }

  void OnDiscoveryConnectionRemoteAccepted(ClientProxy* client,
                                           const Endpoint& endpoint) {
    EXPECT_TRUE(client->HasPendingConnectionToEndpoint(endpoint.id));
    EXPECT_FALSE(client->HasRemoteEndpointResponded(endpoint.id));
    client->RemoteEndpointAcceptedConnection(endpoint.id);
    EXPECT_TRUE(client->HasRemoteEndpointResponded(endpoint.id));
    EXPECT_TRUE(client->RemoteConnectionIsAccepted(endpoint.id));
  }

  void OnDiscoveryConnectionLocalRejected(ClientProxy* client,
                                          const Endpoint& endpoint) {
    EXPECT_TRUE(client->HasPendingConnectionToEndpoint(endpoint.id));
    EXPECT_FALSE(client->HasLocalEndpointResponded(endpoint.id));
    client->LocalEndpointRejectedConnection(endpoint.id);
    EXPECT_TRUE(client->HasLocalEndpointResponded(endpoint.id));
    EXPECT_FALSE(client->LocalConnectionIsAccepted(endpoint.id));
  }

  void OnDiscoveryConnectionRemoteRejected(ClientProxy* client,
                                           const Endpoint& endpoint) {
    EXPECT_TRUE(client->HasPendingConnectionToEndpoint(endpoint.id));
    EXPECT_FALSE(client->HasRemoteEndpointResponded(endpoint.id));
    client->RemoteEndpointRejectedConnection(endpoint.id);
    EXPECT_TRUE(client->HasRemoteEndpointResponded(endpoint.id));
    EXPECT_FALSE(client->RemoteConnectionIsAccepted(endpoint.id));
  }

  void OnDiscoveryConnectionAccepted(ClientProxy* client,
                                     const Endpoint& endpoint) {
    EXPECT_CALL(mock_discovery_connection_.accepted_cb, Call).Times(1);
    EXPECT_TRUE(client->IsConnectionAccepted(endpoint.id));
    client->OnConnectionAccepted(endpoint.id);
  }

  void OnDiscoveryConnectionRejected(ClientProxy* client,
                                     const Endpoint& endpoint) {
    EXPECT_CALL(mock_discovery_connection_.rejected_cb, Call).Times(1);
    EXPECT_TRUE(client->IsConnectionRejected(endpoint.id));
    client->OnConnectionRejected(endpoint.id, {Status::kConnectionRejected});
  }

  void OnDiscoveryBandwidthChanged(ClientProxy* client,
                                   const Endpoint& endpoint) {
    EXPECT_CALL(mock_discovery_connection_.bandwidth_changed_cb, Call).Times(1);
    client->OnBandwidthChanged(endpoint.id, Medium::WIFI_LAN);
  }

  void OnDiscoveryConnectionDisconnected(ClientProxy* client,
                                         const Endpoint& endpoint) {
    EXPECT_CALL(mock_discovery_connection_.disconnected_cb, Call).Times(1);
    client->OnDisconnected(endpoint.id, true);
  }

  void OnPayload(ClientProxy* client, const Endpoint& endpoint) {
    EXPECT_CALL(mock_discovery_payload_.payload_cb, Call).Times(1);
    client->OnPayload(endpoint.id, Payload(payload_bytes_));
  }

  void OnPayloadProgress(ClientProxy* client, const Endpoint& endpoint) {
    EXPECT_CALL(mock_discovery_payload_.payload_progress_cb, Call).Times(1);
    client->OnPayloadProgress(endpoint.id, {});
  }

  MockDiscoveryListener mock_discovery_;
  MockConnectionListener mock_discovery_connection_;
  MockPayloadListener mock_discovery_payload_;

  proto::connections::Medium medium_{proto::connections::Medium::BLUETOOTH};
  std::vector<proto::connections::Medium> mediums_{
      proto::connections::Medium::BLUETOOTH,
  };
  Strategy strategy_{Strategy::kP2pPointToPoint};
  const std::string service_id_{"service"};
  ClientProxy client1_;
  ClientProxy client2_;
  std::string auth_token_ = "auth_token";
  ByteArray raw_auth_token_ = ByteArray(auth_token_);
  ByteArray payload_bytes_{"bytes"};
  ConnectionResponseInfo advertising_connection_info_{
      .authentication_token = auth_token_,
      .raw_authentication_token = raw_auth_token_,
      .is_incoming_connection = true,
  };
  ConnectionListener advertising_connection_listener_;
  ConnectionListener discovery_connection_listener_{
      .initiated_cb = mock_discovery_connection_.initiated_cb.AsStdFunction(),
      .accepted_cb = mock_discovery_connection_.accepted_cb.AsStdFunction(),
      .rejected_cb = mock_discovery_connection_.rejected_cb.AsStdFunction(),
      .disconnected_cb =
          mock_discovery_connection_.disconnected_cb.AsStdFunction(),
      .bandwidth_changed_cb =
          mock_discovery_connection_.bandwidth_changed_cb.AsStdFunction(),
  };
  DiscoveryListener discovery_listener_{
      .endpoint_found_cb = mock_discovery_.endpoint_found_cb.AsStdFunction(),
      .endpoint_lost_cb = mock_discovery_.endpoint_lost_cb.AsStdFunction(),
  };
  PayloadListener payload_listener_{
      .payload_cb = mock_discovery_payload_.payload_cb.AsStdFunction(),
      .payload_progress_cb =
          mock_discovery_payload_.payload_progress_cb.AsStdFunction(),
  };
  ConnectionOptions connection_options_;
};

TEST_P(ClientProxyTest, CanCancelEndpoint) {
  FeatureFlags feature_flags = GetParam();
  MediumEnvironment::Instance().SetFeatureFlags(feature_flags);

  Endpoint advertising_endpoint =
      StartAdvertising(&client1_, advertising_connection_listener_);
  StartDiscovery(&client2_, discovery_listener_);
  OnDiscoveryEndpointFound(&client2_, advertising_endpoint);
  OnDiscoveryConnectionInitiated(&client2_, advertising_endpoint);

  EXPECT_FALSE(
      client2_.GetCancellationFlag(advertising_endpoint.id)->Cancelled());

  client2_.CancelEndpoint(advertising_endpoint.id);

  // If FeatureFlag is disabled, Cancelled is false as no-op.
  if (!feature_flags.enable_cancellation_flag) {
    EXPECT_FALSE(
        client2_.GetCancellationFlag(advertising_endpoint.id)->Cancelled());
  } else {
    // The Cancelled is always true as the default flag being returned.
    EXPECT_TRUE(
        client2_.GetCancellationFlag(advertising_endpoint.id)->Cancelled());
  }
}

TEST_P(ClientProxyTest, CanCancelAllEndpoints) {
  FeatureFlags feature_flags = GetParam();
  MediumEnvironment::Instance().SetFeatureFlags(feature_flags);

  Endpoint advertising_endpoint =
      StartAdvertising(&client1_, advertising_connection_listener_);
  StartDiscovery(&client2_, discovery_listener_);
  OnDiscoveryEndpointFound(&client2_, advertising_endpoint);
  OnDiscoveryConnectionInitiated(&client2_, advertising_endpoint);

  EXPECT_FALSE(
      client2_.GetCancellationFlag(advertising_endpoint.id)->Cancelled());

  client2_.CancelAllEndpoints();

  // If FeatureFlag is disabled, Cancelled is false as no-op.
  if (!feature_flags.enable_cancellation_flag) {
    EXPECT_FALSE(
        client2_.GetCancellationFlag(advertising_endpoint.id)->Cancelled());
  } else {
    // The Cancelled is always true as the default flag being returned.
    EXPECT_TRUE(
        client2_.GetCancellationFlag(advertising_endpoint.id)->Cancelled());
  }
}

TEST_P(ClientProxyTest, CanCancelAllEndpointsWithDifferentEndpoint) {
  FeatureFlags feature_flags = GetParam();
  MediumEnvironment::Instance().SetFeatureFlags(feature_flags);

  ConnectionListener advertising_connection_listener_2;
  ConnectionListener advertising_connection_listener_3;
  ClientProxy client3;

  StartDiscovery(&client1_, discovery_listener_);
  Endpoint advertising_endpoint_2 =
      StartAdvertising(&client2_, advertising_connection_listener_2);
  Endpoint advertising_endpoint_3 =
      StartAdvertising(&client3, advertising_connection_listener_3);
  OnDiscoveryEndpointFound(&client1_, advertising_endpoint_2);
  OnDiscoveryConnectionInitiated(&client1_, advertising_endpoint_2);
  OnDiscoveryEndpointFound(&client1_, advertising_endpoint_3);
  OnDiscoveryConnectionInitiated(&client1_, advertising_endpoint_3);

  // The CancellationFlag of endpoint_2 and endpoint_3 have been added. Default
  // Cancelled is false.
  EXPECT_FALSE(
      client1_.GetCancellationFlag(advertising_endpoint_2.id)->Cancelled());
  EXPECT_FALSE(
      client1_.GetCancellationFlag(advertising_endpoint_3.id)->Cancelled());

  client1_.CancelAllEndpoints();

  if (!feature_flags.enable_cancellation_flag) {
    // The CancellationFlag of endpoint_2 and endpoint_3 will not be removed
    // since it is not added. The default flag returned as Cancelled being true,
    // but Cancelled requested is false since the FeatureFlag is off.
    EXPECT_FALSE(
        client1_.GetCancellationFlag(advertising_endpoint_2.id)->Cancelled());
    EXPECT_FALSE(
        client1_.GetCancellationFlag(advertising_endpoint_3.id)->Cancelled());
  } else {
    // Expect the CancellationFlag of endpoint_2 and endpoint_3 has been
    // removed. The Cancelled is always true as the default flag being returned.
    EXPECT_TRUE(
        client1_.GetCancellationFlag(advertising_endpoint_2.id)->Cancelled());
    EXPECT_TRUE(
        client1_.GetCancellationFlag(advertising_endpoint_3.id)->Cancelled());
  }
}

INSTANTIATE_TEST_SUITE_P(ParametrisedClientProxyTest, ClientProxyTest,
                         ::testing::ValuesIn(kTestCases));

TEST_F(ClientProxyTest, ConstructorDestructorWorks) { SUCCEED(); }

TEST_F(ClientProxyTest, ClientIdIsUnique) {
  EXPECT_NE(client1_.GetClientId(), client2_.GetClientId());
}

TEST_F(ClientProxyTest, GeneratedEndpointIdIsUnique) {
  EXPECT_NE(client1_.GetLocalEndpointId(), client2_.GetLocalEndpointId());
}

TEST_F(ClientProxyTest, ResetClearsState) {
  client1_.Reset();
  EXPECT_FALSE(client1_.IsAdvertising());
  EXPECT_FALSE(client1_.IsDiscovering());
  EXPECT_TRUE(client1_.GetAdvertisingServiceId().empty());
  EXPECT_TRUE(client1_.GetDiscoveryServiceId().empty());
}

TEST_F(ClientProxyTest, StartedAdvertisingChangesStateFromIdle) {
  client1_.StartedAdvertising(service_id_, strategy_, {}, {});

  EXPECT_TRUE(client1_.IsAdvertising());
  EXPECT_FALSE(client1_.IsDiscovering());
  EXPECT_EQ(client1_.GetAdvertisingServiceId(), service_id_);
  EXPECT_TRUE(client1_.GetDiscoveryServiceId().empty());
}

TEST_F(ClientProxyTest, StartedDiscoveryChangesStateFromIdle) {
  client1_.StartedDiscovery(service_id_, strategy_, {}, {});

  EXPECT_FALSE(client1_.IsAdvertising());
  EXPECT_TRUE(client1_.IsDiscovering());
  EXPECT_TRUE(client1_.GetAdvertisingServiceId().empty());
  EXPECT_EQ(client1_.GetDiscoveryServiceId(), service_id_);
}

TEST_F(ClientProxyTest, OnEndpointFoundFiresNotificationInDiscovery) {
  Endpoint advertising_endpoint =
      StartAdvertising(&client1_, advertising_connection_listener_);
  StartDiscovery(&client2_, discovery_listener_);
  OnDiscoveryEndpointFound(&client2_, advertising_endpoint);
}

TEST_F(ClientProxyTest, OnEndpointLostFiresNotificationInDiscovery) {
  Endpoint advertising_endpoint =
      StartAdvertising(&client1_, advertising_connection_listener_);
  StartDiscovery(&client2_, discovery_listener_);
  OnDiscoveryEndpointFound(&client2_, advertising_endpoint);
  OnDiscoveryEndpointLost(&client2_, advertising_endpoint);
}

TEST_F(ClientProxyTest, OnConnectionInitiatedFiresNotificationInDiscovery) {
  Endpoint advertising_endpoint =
      StartAdvertising(&client1_, advertising_connection_listener_);
  StartDiscovery(&client2_, discovery_listener_);
  OnDiscoveryEndpointFound(&client2_, advertising_endpoint);
  OnDiscoveryConnectionInitiated(&client2_, advertising_endpoint);
}

TEST_F(ClientProxyTest, OnBandwidthChangedFiresNotificationInDiscovery) {
  Endpoint advertising_endpoint =
      StartAdvertising(&client1_, advertising_connection_listener_);
  StartDiscovery(&client2_, discovery_listener_);
  OnDiscoveryEndpointFound(&client2_, advertising_endpoint);
  OnDiscoveryConnectionInitiated(&client2_, advertising_endpoint);
  OnDiscoveryConnectionLocalAccepted(&client2_, advertising_endpoint);
  OnDiscoveryConnectionRemoteAccepted(&client2_, advertising_endpoint);
  OnDiscoveryConnectionAccepted(&client2_, advertising_endpoint);
  OnDiscoveryBandwidthChanged(&client2_, advertising_endpoint);
}

TEST_F(ClientProxyTest, OnDisconnectedFiresNotificationInDiscovery) {
  Endpoint advertising_endpoint =
      StartAdvertising(&client1_, advertising_connection_listener_);
  StartDiscovery(&client2_, discovery_listener_);
  OnDiscoveryEndpointFound(&client2_, advertising_endpoint);
  OnDiscoveryConnectionInitiated(&client2_, advertising_endpoint);
  OnDiscoveryConnectionDisconnected(&client2_, advertising_endpoint);
}

TEST_F(ClientProxyTest, LocalEndpointAcceptedConnectionChangesState) {
  Endpoint advertising_endpoint =
      StartAdvertising(&client1_, advertising_connection_listener_);
  StartDiscovery(&client2_, discovery_listener_);
  OnDiscoveryEndpointFound(&client2_, advertising_endpoint);
  OnDiscoveryConnectionInitiated(&client2_, advertising_endpoint);
  OnDiscoveryConnectionLocalAccepted(&client2_, advertising_endpoint);
}

TEST_F(ClientProxyTest, LocalEndpointRejectedConnectionChangesState) {
  Endpoint advertising_endpoint =
      StartAdvertising(&client1_, advertising_connection_listener_);
  StartDiscovery(&client2_, discovery_listener_);
  OnDiscoveryEndpointFound(&client2_, advertising_endpoint);
  OnDiscoveryConnectionInitiated(&client2_, advertising_endpoint);
  OnDiscoveryConnectionLocalRejected(&client2_, advertising_endpoint);
}

TEST_F(ClientProxyTest, RemoteEndpointAcceptedConnectionChangesState) {
  Endpoint advertising_endpoint =
      StartAdvertising(&client1_, advertising_connection_listener_);
  StartDiscovery(&client2_, discovery_listener_);
  OnDiscoveryEndpointFound(&client2_, advertising_endpoint);
  OnDiscoveryConnectionInitiated(&client2_, advertising_endpoint);
  OnDiscoveryConnectionRemoteAccepted(&client2_, advertising_endpoint);
}

TEST_F(ClientProxyTest, RemoteEndpointRejectedConnectionChangesState) {
  Endpoint advertising_endpoint =
      StartAdvertising(&client1_, advertising_connection_listener_);
  StartDiscovery(&client2_, discovery_listener_);
  OnDiscoveryEndpointFound(&client2_, advertising_endpoint);
  OnDiscoveryConnectionInitiated(&client2_, advertising_endpoint);
  OnDiscoveryConnectionRemoteRejected(&client2_, advertising_endpoint);
}

TEST_F(ClientProxyTest, OnPayloadChangesState) {
  Endpoint advertising_endpoint =
      StartAdvertising(&client1_, advertising_connection_listener_);
  StartDiscovery(&client2_, discovery_listener_);
  OnDiscoveryEndpointFound(&client2_, advertising_endpoint);
  OnDiscoveryConnectionInitiated(&client2_, advertising_endpoint);
  OnDiscoveryConnectionLocalAccepted(&client2_, advertising_endpoint);
  OnDiscoveryConnectionRemoteAccepted(&client2_, advertising_endpoint);
  OnDiscoveryConnectionAccepted(&client2_, advertising_endpoint);
  OnPayload(&client2_, advertising_endpoint);
}

TEST_F(ClientProxyTest, OnPayloadProgressChangesState) {
  Endpoint advertising_endpoint =
      StartAdvertising(&client1_, advertising_connection_listener_);
  StartDiscovery(&client2_, discovery_listener_);
  OnDiscoveryEndpointFound(&client2_, advertising_endpoint);
  OnDiscoveryConnectionInitiated(&client2_, advertising_endpoint);
  OnDiscoveryConnectionLocalAccepted(&client2_, advertising_endpoint);
  OnDiscoveryConnectionRemoteAccepted(&client2_, advertising_endpoint);
  OnDiscoveryConnectionAccepted(&client2_, advertising_endpoint);
  OnPayloadProgress(&client2_, advertising_endpoint);
}

TEST_F(ClientProxyTest,
       EndpointIdCacheWhenHighVizAdvertisementAgainImmediately) {
  ConnectionOptions advertising_options{.strategy = strategy_,
                                        .allowed =
                                            {
                                                .bluetooth = true,
                                            },
                                        .low_power = false};

  Endpoint advertising_endpoint_1 = StartAdvertising(
      &client1_, advertising_connection_listener_, advertising_options);

  StopAdvertising(&client1_);

  // Advertise immediately.
  Endpoint advertising_endpoint_2 = StartAdvertising(
      &client1_, advertising_connection_listener_, advertising_options);

  EXPECT_EQ(advertising_endpoint_1.id, advertising_endpoint_2.id);
}

TEST_F(ClientProxyTest,
       EndpointIdRotateWhenHighVizAdvertisementAgainForAWhile) {
  ConnectionOptions advertising_options{.strategy = strategy_,
                                        .allowed =
                                            {
                                                .bluetooth = true,
                                            },
                                        .low_power = false};

  Endpoint advertising_endpoint_1 = StartAdvertising(
      &client1_, advertising_connection_listener_, advertising_options);

  StopAdvertising(&client1_);

  // Wait to expire and then advertise.
  absl::SleepFor(ClientProxy::kHighPowerAdvertisementEndpointIdCacheTimeout +
                 absl::Milliseconds(10));
  Endpoint advertising_endpoint_2 = StartAdvertising(
      &client1_, advertising_connection_listener_, advertising_options);

  EXPECT_NE(advertising_endpoint_1.id, advertising_endpoint_2.id);
}

TEST_F(ClientProxyTest,
       EndpointIdRotateWhenLowVizAdvertisementAfterHighVizAdvertisement) {
  ConnectionOptions high_viz_advertising_options{.strategy = strategy_,
                                                 .allowed =
                                                     {
                                                         .bluetooth = true,
                                                     },
                                                 .low_power = false};
  Endpoint advertising_endpoint_1 =
      StartAdvertising(&client1_, advertising_connection_listener_,
                       high_viz_advertising_options);

  StopAdvertising(&client1_);

  ConnectionOptions low_viz_advertising_options{.strategy = strategy_,
                                                .low_power = true};

  Endpoint advertising_endpoint_2 = StartAdvertising(
      &client1_, advertising_connection_listener_, low_viz_advertising_options);

  EXPECT_NE(advertising_endpoint_1.id, advertising_endpoint_2.id);
}

// Tests endpoint_id rotates when discover.
TEST_F(ClientProxyTest, EndpointIdRotateWhenStartDiscovery) {
  ConnectionOptions advertising_options{.strategy = strategy_,
                                        .allowed =
                                            {
                                                .bluetooth = true,
                                            },
                                        .low_power = false};

  Endpoint advertising_endpoint_1 = StartAdvertising(
      &client1_, advertising_connection_listener_, advertising_options);

  StopAdvertising(&client1_);
  StartDiscovery(&client1_, discovery_listener_);

  Endpoint advertising_endpoint_2 = StartAdvertising(
      &client1_, advertising_connection_listener_, advertising_options);

  EXPECT_NE(advertising_endpoint_1.id, advertising_endpoint_2.id);
}

// Tests the low visibility mode with bluetooth disabled advertisment.
TEST_F(ClientProxyTest,
       EndpointIdRotateWhenLowVizAdvertisementWithBluetoothDisabled) {
  ConnectionOptions advertising_options{.strategy = strategy_,
                                        .allowed = {
                                            .bluetooth = false,
                                        }};

  Endpoint advertising_endpoint_1 = StartAdvertising(
      &client1_, advertising_connection_listener_, advertising_options);

  StopAdvertising(&client1_);

  Endpoint advertising_endpoint_2 = StartAdvertising(
      &client1_, advertising_connection_listener_, advertising_options);

  EXPECT_NE(advertising_endpoint_1.id, advertising_endpoint_2.id);
}

// Tests the low visibility mode with low power advertisment.
TEST_F(ClientProxyTest, EndpointIdRotateWhenLowVizAdvertisementWithLowPower) {
  ConnectionOptions advertising_options{.strategy = strategy_,
                                        .low_power = true};

  Endpoint advertising_endpoint_1 = StartAdvertising(
      &client1_, advertising_connection_listener_, advertising_options);

  StopAdvertising(&client1_);

  Endpoint advertising_endpoint_2 = StartAdvertising(
      &client1_, advertising_connection_listener_, advertising_options);

  EXPECT_NE(advertising_endpoint_1.id, advertising_endpoint_2.id);
}

}  // namespace
}  // namespace connections
}  // namespace nearby
}  // namespace location
