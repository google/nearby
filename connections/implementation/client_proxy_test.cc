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

#include "connections/implementation/client_proxy.h"

#include <cstdint>
#include <cstdio>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "connections/listeners.h"
#include "connections/strategy.h"
#include "connections/v3/bandwidth_info.h"
#include "connections/v3/connection_listening_options.h"
#include "connections/v3/connections_device_provider.h"
#include "internal/analytics/event_logger.h"
#include "internal/interop/device_provider.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/feature_flags.h"
#include "internal/platform/medium_environment.h"
#include "proto/connections_enums.pb.h"

namespace nearby {
namespace connections {
namespace {

using ::location::nearby::analytics::proto::ConnectionsLog;
using ::location::nearby::connections::OsInfo;
using ::location::nearby::proto::connections::CLIENT_SESSION;
using ::location::nearby::proto::connections::START_CLIENT_SESSION;
using ::location::nearby::proto::connections::STOP_CLIENT_SESSION;
using ::testing::MockFunction;
using ::testing::StrictMock;

constexpr FeatureFlags::Flags kTestCases[] = {
    FeatureFlags::Flags{
        .enable_cancellation_flag = true,
    },
    FeatureFlags::Flags{
        .enable_cancellation_flag = false,
    },
};

class FakeEventLogger : public ::nearby::analytics::EventLogger {
 public:
  explicit FakeEventLogger() = default;

  void Log(const ::google::protobuf::MessageLite& message) override {
    ConnectionsLog log;
    log.CheckTypeAndMergeFrom(message);
    MutexLock lock(&mutex_);
    logs_.push_back(std::move(log));
  }

  int GetCompleteClientSessionCount() {
    MutexLock lock(&mutex_);
    bool has_start_client_session = false;
    bool has_client_session = false;
    int session_count = 0;
    // We expect series of START_CLIENT_SESSION, CLIENT_SESSION and
    // STOP_CLIENT_SESSION events, possibly interleaved with other events.
    for (const auto& log : logs_) {
      if (log.event_type() == START_CLIENT_SESSION) {
        EXPECT_FALSE(has_start_client_session);
        EXPECT_FALSE(has_client_session);
        has_start_client_session = true;
      } else if (log.event_type() == CLIENT_SESSION) {
        EXPECT_TRUE(has_start_client_session);
        EXPECT_FALSE(has_client_session);
        has_client_session = true;
      } else if (log.event_type() == STOP_CLIENT_SESSION) {
        EXPECT_TRUE(has_start_client_session);
        EXPECT_TRUE(has_client_session);
        has_start_client_session = false;
        has_client_session = false;
        ++session_count;
      }
    }
    return session_count;
  }

  Mutex mutex_;
  std::vector<ConnectionsLog> logs_;
};

class MockDeviceProvider : public nearby::NearbyDeviceProvider {
 public:
  MOCK_METHOD((const NearbyDevice*), GetLocalDevice, (), (override));
};

class ClientProxyTest : public ::testing::TestWithParam<FeatureFlags::Flags> {
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
        MockFunction<void(absl::string_view endpoint_id, Payload payload)>>
        payload_cb;
    StrictMock<MockFunction<void(absl::string_view endpoint_id,
                                 const PayloadProgressInfo& info)>>
        payload_progress_cb;
  };

  struct Endpoint {
    ByteArray info;
    std::string id;
  };

  bool ShouldEnterHighVisibilityMode(
      const AdvertisingOptions& advertising_options) {
    return !advertising_options.low_power &&
           advertising_options.allowed.bluetooth;
  }

  Endpoint StartAdvertising(
      ClientProxy* client, ConnectionListener listener,
      AdvertisingOptions advertising_options = AdvertisingOptions{}) {
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

  void StopAdvertising(ClientProxy* client) {
    client->StoppedAdvertising();
    EXPECT_FALSE(client->IsAdvertising());
  }

  void OnAdvertisingConnectionInitiated(ClientProxy* client,
                                        const Endpoint& endpoint) {
    EXPECT_CALL(mock_advertising_connection_.initiated_cb, Call).Times(1);
    const std::string auth_token{"auth_token"};
    const ByteArray raw_auth_token{auth_token};
    const std::string connection_token{"conntokn"};
    discovery_connection_info_.remote_endpoint_info = endpoint.info;
    client->OnConnectionInitiated(
        endpoint.id, discovery_connection_info_, connection_options_,
        advertising_connection_listener_, connection_token);
    EXPECT_TRUE(client->HasPendingConnectionToEndpoint(endpoint.id));
  }

  Endpoint StartListeningForIncomingConnections(
      ClientProxy* client, v3::ConnectionListener listener,
      v3::ConnectionListeningOptions options = {}) {
    Endpoint endpoint{
        .info = ByteArray{"advertising endpoint name"},
        .id = client->GetLocalEndpointId(),
    };
    client->StartedListeningForIncomingConnections(
        service_id_, strategy_, std::move(listener), options);
    return endpoint;
  }

  void StopListeningForIncomingConnections(ClientProxy* client) {
    client->StoppedListeningForIncomingConnections();
    EXPECT_FALSE(client->IsListeningForIncomingConnections());
  }

  Endpoint StartDiscovery(ClientProxy* client, DiscoveryListener listener) {
    Endpoint endpoint{
        .info = ByteArray{"discovery endpoint name"},
        .id = client->GetLocalEndpointId(),
    };
    client->StartedDiscovery(service_id_, strategy_, std::move(listener),
                             absl::MakeSpan(mediums_));
    return endpoint;
  }

  void StopDiscovery(ClientProxy* client) {
    client->StoppedDiscovery();
    EXPECT_FALSE(client->IsDiscovering());
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
    const std::string connection_token{"conntokn"};
    advertising_connection_info_.remote_endpoint_info = endpoint.info;
    client->OnConnectionInitiated(
        endpoint.id, advertising_connection_info_, connection_options_,
        discovery_connection_listener_, connection_token);
    EXPECT_TRUE(client->HasPendingConnectionToEndpoint(endpoint.id));
  }

  void OnDiscoveryConnectionLocalAccepted(ClientProxy* client,
                                          const Endpoint& endpoint) {
    EXPECT_TRUE(client->HasPendingConnectionToEndpoint(endpoint.id));
    EXPECT_FALSE(client->HasLocalEndpointResponded(endpoint.id));
    client->LocalEndpointAcceptedConnection(
        endpoint.id,
        {
            .payload_cb = mock_discovery_payload_.payload_cb.AsStdFunction(),
            .payload_progress_cb =
                mock_discovery_payload_.payload_progress_cb.AsStdFunction(),
        });
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

  MockConnectionListener mock_advertising_connection_;

  MockDiscoveryListener mock_discovery_;
  MockConnectionListener mock_discovery_connection_;
  MockPayloadListener mock_discovery_payload_;

  location::nearby::proto::connections::Medium medium_{
      location::nearby::proto::connections::Medium::BLUETOOTH};
  std::vector<location::nearby::proto::connections::Medium> mediums_{
      location::nearby::proto::connections::Medium::BLUETOOTH,
  };
  Strategy strategy_{Strategy::kP2pPointToPoint};
  const std::string service_id_{"service"};
  FakeEventLogger event_logger1_;
  FakeEventLogger event_logger2_;
  ClientProxy client1_{&event_logger1_};
  ClientProxy client2_{&event_logger2_};
  std::string auth_token_ = "auth_token";
  ByteArray raw_auth_token_ = ByteArray(auth_token_);
  ByteArray payload_bytes_{"bytes"};
  ConnectionResponseInfo advertising_connection_info_{
      .authentication_token = auth_token_,
      .raw_authentication_token = raw_auth_token_,
      .is_incoming_connection = true,
  };
  ConnectionListener advertising_connection_listener_{
      .initiated_cb = mock_advertising_connection_.initiated_cb.AsStdFunction(),
  };
  ConnectionResponseInfo discovery_connection_info_{
      .authentication_token = auth_token_,
      .raw_authentication_token = raw_auth_token_,
      .is_incoming_connection = false,
  };
  ConnectionListener discovery_connection_listener_{
      .initiated_cb = mock_discovery_connection_.initiated_cb.AsStdFunction(),
      .accepted_cb = mock_discovery_connection_.accepted_cb.AsStdFunction(),
      .rejected_cb = mock_discovery_connection_.rejected_cb.AsStdFunction(),
      .disconnected_cb =
          mock_discovery_connection_.disconnected_cb.AsStdFunction(),
      .bandwidth_changed_cb =
          mock_discovery_connection_.bandwidth_changed_cb.AsStdFunction(),
  };
  DiscoveryListener GetDiscoveryListener() {
    return DiscoveryListener{
        .endpoint_found_cb = mock_discovery_.endpoint_found_cb.AsStdFunction(),
        .endpoint_lost_cb = mock_discovery_.endpoint_lost_cb.AsStdFunction(),
    };
  }
  ConnectionOptions connection_options_;
  AdvertisingOptions advertising_options_;
  DiscoveryOptions discovery_options_;
};

// Regression test for b/279962714.
TEST_P(ClientProxyTest, CanCancelEndpoint) {
  FeatureFlags::Flags feature_flags = GetParam();
  MediumEnvironment::Instance().SetFeatureFlags(feature_flags);

  Endpoint advertising_endpoint =
      StartAdvertising(&client1_, advertising_connection_listener_);
  StartDiscovery(&client2_, GetDiscoveryListener());
  OnDiscoveryEndpointFound(&client2_, advertising_endpoint);
  OnDiscoveryConnectionInitiated(&client2_, advertising_endpoint);

  // `CancellationFlag` pointers are passed to other classes in Nearby
  // Connections, and by using the pointers directly, we test their
  // consumption of `CancellationFlag` pointers.
  CancellationFlag* cancellation_flag =
      client2_.GetCancellationFlag(advertising_endpoint.id);

  EXPECT_FALSE(
      client2_.GetCancellationFlag(advertising_endpoint.id)->Cancelled());
  EXPECT_FALSE(cancellation_flag->Cancelled());

  client2_.CancelEndpoint(advertising_endpoint.id);

  // If FeatureFlag is disabled, Cancelled is false as no-op.
  if (!feature_flags.enable_cancellation_flag) {
    EXPECT_FALSE(
        client2_.GetCancellationFlag(advertising_endpoint.id)->Cancelled());
    EXPECT_FALSE(cancellation_flag->Cancelled());
  } else {
    // The Cancelled is always true as the default flag being returned.
    EXPECT_TRUE(
        client2_.GetCancellationFlag(advertising_endpoint.id)->Cancelled());
    EXPECT_TRUE(cancellation_flag->Cancelled());
  }
}

// Regression test for b/279962714.
TEST_P(ClientProxyTest, CanCancelAllEndpoints) {
  FeatureFlags::Flags feature_flags = GetParam();
  MediumEnvironment::Instance().SetFeatureFlags(feature_flags);

  Endpoint advertising_endpoint =
      StartAdvertising(&client1_, advertising_connection_listener_);
  StartDiscovery(&client2_, GetDiscoveryListener());
  OnDiscoveryEndpointFound(&client2_, advertising_endpoint);
  OnDiscoveryConnectionInitiated(&client2_, advertising_endpoint);

  // `CancellationFlag` pointers are passed to other classes in Nearby
  // Connections, and by using the pointers directly, we test their
  // consumption of `CancellationFlag` pointers.
  CancellationFlag* cancellation_flag =
      client2_.GetCancellationFlag(advertising_endpoint.id);

  EXPECT_FALSE(
      client2_.GetCancellationFlag(advertising_endpoint.id)->Cancelled());
  EXPECT_FALSE(cancellation_flag->Cancelled());

  client2_.CancelAllEndpoints();

  // If FeatureFlag is disabled, Cancelled is false as no-op.
  if (!feature_flags.enable_cancellation_flag) {
    EXPECT_FALSE(
        client2_.GetCancellationFlag(advertising_endpoint.id)->Cancelled());
    EXPECT_FALSE(cancellation_flag->Cancelled());
  } else {
    // The Cancelled is always true as the default flag being returned.
    EXPECT_TRUE(
        client2_.GetCancellationFlag(advertising_endpoint.id)->Cancelled());
    EXPECT_TRUE(cancellation_flag->Cancelled());
  }
}

TEST_P(ClientProxyTest, CanCancelAllEndpointsWithDifferentEndpoint) {
  FeatureFlags::Flags feature_flags = GetParam();
  MediumEnvironment::Instance().SetFeatureFlags(feature_flags);

  ConnectionListener advertising_connection_listener_2;
  ConnectionListener advertising_connection_listener_3;
  ClientProxy client3;

  StartDiscovery(&client1_, GetDiscoveryListener());
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

TEST_F(ClientProxyTest, DumpString) {
  std::string expect = absl::StrFormat(
      "Nearby Connections State\n"
      "  Client ID: %d\n"
      "  Local Endpoint ID: %s\n"
      "  High Visibility Mode: false\n"
      "  Is Advertising: false\n"
      "  Is Discovering: false\n"
      "  Advertising Service ID: \n"
      "  Discovery Service ID: \n"
      "  Connections: \n"
      "  Discovered endpoint IDs: \n",
      client1_.GetClientId(), client1_.GetLocalEndpointId());
  std::string dump = client1_.Dump();
  EXPECT_EQ(dump, expect);
}

TEST_F(ClientProxyTest, GeneratedEndpointIdIsUnique) {
  EXPECT_NE(client1_.GetLocalEndpointId(), client2_.GetLocalEndpointId());
}

TEST_F(ClientProxyTest, GeneratedEndpointIdIsUniqueWithDeviceProvider) {
  client1_.RegisterConnectionsDeviceProvider(
      std::make_unique<v3::ConnectionsDeviceProvider>(
          v3::ConnectionsDeviceProvider("", {})));
  client2_.RegisterConnectionsDeviceProvider(
      std::make_unique<v3::ConnectionsDeviceProvider>(
          v3::ConnectionsDeviceProvider("", {})));
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
  StartDiscovery(&client2_, GetDiscoveryListener());
  OnDiscoveryEndpointFound(&client2_, advertising_endpoint);
}

TEST_F(ClientProxyTest, OnEndpointLostFiresNotificationInDiscovery) {
  Endpoint advertising_endpoint =
      StartAdvertising(&client1_, advertising_connection_listener_);
  StartDiscovery(&client2_, GetDiscoveryListener());
  OnDiscoveryEndpointFound(&client2_, advertising_endpoint);
  OnDiscoveryEndpointLost(&client2_, advertising_endpoint);
}

TEST_F(ClientProxyTest, OnConnectionInitiatedFiresNotificationInDiscovery) {
  Endpoint advertising_endpoint =
      StartAdvertising(&client1_, advertising_connection_listener_);
  StartDiscovery(&client2_, GetDiscoveryListener());
  OnDiscoveryEndpointFound(&client2_, advertising_endpoint);
  OnDiscoveryConnectionInitiated(&client2_, advertising_endpoint);
}

TEST_F(ClientProxyTest, OnBandwidthChangedFiresNotificationInDiscovery) {
  Endpoint advertising_endpoint =
      StartAdvertising(&client1_, advertising_connection_listener_);
  StartDiscovery(&client2_, GetDiscoveryListener());
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
  StartDiscovery(&client2_, GetDiscoveryListener());
  OnDiscoveryEndpointFound(&client2_, advertising_endpoint);
  OnDiscoveryConnectionInitiated(&client2_, advertising_endpoint);
  OnDiscoveryConnectionDisconnected(&client2_, advertising_endpoint);
}

TEST_F(ClientProxyTest, LocalEndpointAcceptedConnectionChangesState) {
  Endpoint advertising_endpoint =
      StartAdvertising(&client1_, advertising_connection_listener_);
  StartDiscovery(&client2_, GetDiscoveryListener());
  OnDiscoveryEndpointFound(&client2_, advertising_endpoint);
  OnDiscoveryConnectionInitiated(&client2_, advertising_endpoint);
  OnDiscoveryConnectionLocalAccepted(&client2_, advertising_endpoint);
}

TEST_F(ClientProxyTest, LocalEndpointRejectedConnectionChangesState) {
  Endpoint advertising_endpoint =
      StartAdvertising(&client1_, advertising_connection_listener_);
  StartDiscovery(&client2_, GetDiscoveryListener());
  OnDiscoveryEndpointFound(&client2_, advertising_endpoint);
  OnDiscoveryConnectionInitiated(&client2_, advertising_endpoint);
  OnDiscoveryConnectionLocalRejected(&client2_, advertising_endpoint);
}

TEST_F(ClientProxyTest, RemoteEndpointAcceptedConnectionChangesState) {
  Endpoint advertising_endpoint =
      StartAdvertising(&client1_, advertising_connection_listener_);
  StartDiscovery(&client2_, GetDiscoveryListener());
  OnDiscoveryEndpointFound(&client2_, advertising_endpoint);
  OnDiscoveryConnectionInitiated(&client2_, advertising_endpoint);
  OnDiscoveryConnectionRemoteAccepted(&client2_, advertising_endpoint);
}

TEST_F(ClientProxyTest, RemoteEndpointRejectedConnectionChangesState) {
  Endpoint advertising_endpoint =
      StartAdvertising(&client1_, advertising_connection_listener_);
  StartDiscovery(&client2_, GetDiscoveryListener());
  OnDiscoveryEndpointFound(&client2_, advertising_endpoint);
  OnDiscoveryConnectionInitiated(&client2_, advertising_endpoint);
  OnDiscoveryConnectionRemoteRejected(&client2_, advertising_endpoint);
}

TEST_F(ClientProxyTest, OnPayloadChangesState) {
  Endpoint advertising_endpoint =
      StartAdvertising(&client1_, advertising_connection_listener_);
  StartDiscovery(&client2_, GetDiscoveryListener());
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
  StartDiscovery(&client2_, GetDiscoveryListener());
  OnDiscoveryEndpointFound(&client2_, advertising_endpoint);
  OnDiscoveryConnectionInitiated(&client2_, advertising_endpoint);
  OnDiscoveryConnectionLocalAccepted(&client2_, advertising_endpoint);
  OnDiscoveryConnectionRemoteAccepted(&client2_, advertising_endpoint);
  OnDiscoveryConnectionAccepted(&client2_, advertising_endpoint);
  OnPayloadProgress(&client2_, advertising_endpoint);
}

TEST_F(ClientProxyTest,
       EndpointIdCacheWhenHighVizAdvertisementAgainImmediately) {
  BooleanMediumSelector booleanMediumSelector;
  booleanMediumSelector.bluetooth = true;

  AdvertisingOptions advertising_options{
      {
          strategy_,
          booleanMediumSelector,
      },
      false,  // auto_upgrade_bandwidth
      false,  // enforce_topology_constraints
      false,  // low_power
  };

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
  BooleanMediumSelector booleanMediumSelector;
  booleanMediumSelector.bluetooth = true;

  AdvertisingOptions advertising_options{
      {
          strategy_,
          booleanMediumSelector,
      },
      false,  // auto_upgrade_bandwidth
      false,  // enforce_topology_constraints
      false,  // low_power
  };

  Endpoint advertising_endpoint_1 = StartAdvertising(
      &client1_, advertising_connection_listener_, advertising_options);

  StopAdvertising(&client1_);

  // Wait to expire and then advertise.
  absl::SleepFor(ClientProxy::kHighPowerAdvertisementEndpointIdCacheTimeout +
                 absl::Milliseconds(100));
  Endpoint advertising_endpoint_2 = StartAdvertising(
      &client1_, advertising_connection_listener_, advertising_options);

  EXPECT_NE(advertising_endpoint_1.id, advertising_endpoint_2.id);
}

TEST_F(ClientProxyTest,
       EndpointIdRotateWhenLowVizAdvertisementAfterHighVizAdvertisement) {
  BooleanMediumSelector booleanMediumSelector;
  booleanMediumSelector.bluetooth = true;

  AdvertisingOptions high_viz_advertising_options{
      {
          strategy_,
          booleanMediumSelector,
      },
      false,  // auto_upgrade_bandwidth
      false,  // enforce_topology_constraints
      false,  // low_power
  };
  Endpoint advertising_endpoint_1 =
      StartAdvertising(&client1_, advertising_connection_listener_,
                       high_viz_advertising_options);

  StopAdvertising(&client1_);

  AdvertisingOptions low_viz_advertising_options{
      {
          strategy_,
          booleanMediumSelector,
      },
      false,  // auto_upgrade_bandwidth
      false,  // enforce_topology_constraints
      true,   // low_power
  };

  Endpoint advertising_endpoint_2 = StartAdvertising(
      &client1_, advertising_connection_listener_, low_viz_advertising_options);

  EXPECT_NE(advertising_endpoint_1.id, advertising_endpoint_2.id);
}

// Tests endpoint_id rotates when discover.
TEST_F(ClientProxyTest, EndpointIdRotateWhenStartDiscovery) {
  BooleanMediumSelector booleanMediumSelector;
  booleanMediumSelector.bluetooth = true;

  AdvertisingOptions advertising_options{
      {
          strategy_,
          booleanMediumSelector,
      },
      false,  // auto_upgrade_bandwidth
      false,  // enforce_topology_constraints
      false,  // low_power
  };

  Endpoint advertising_endpoint_1 = StartAdvertising(
      &client1_, advertising_connection_listener_, advertising_options);

  StopAdvertising(&client1_);
  StartDiscovery(&client1_, GetDiscoveryListener());

  Endpoint advertising_endpoint_2 = StartAdvertising(
      &client1_, advertising_connection_listener_, advertising_options);

  EXPECT_NE(advertising_endpoint_1.id, advertising_endpoint_2.id);
}

// Tests the low visibility mode with bluetooth disabled advertisment.
TEST_F(ClientProxyTest,
       EndpointIdRotateWhenLowVizAdvertisementWithBluetoothDisabled) {
  BooleanMediumSelector booleanMediumSelector;
  booleanMediumSelector.bluetooth = false;

  AdvertisingOptions advertising_options{
      {
          strategy_,
          booleanMediumSelector,
      },
      false,  // auto_upgrade_bandwidth
      false,  // enforce_topology_constraints
      false,  // low_power
  };

  Endpoint advertising_endpoint_1 = StartAdvertising(
      &client1_, advertising_connection_listener_, advertising_options);

  StopAdvertising(&client1_);

  Endpoint advertising_endpoint_2 = StartAdvertising(
      &client1_, advertising_connection_listener_, advertising_options);

  EXPECT_NE(advertising_endpoint_1.id, advertising_endpoint_2.id);
}

// Tests the low visibility mode with low power advertisment.
TEST_F(ClientProxyTest, EndpointIdRotateWhenLowVizAdvertisementWithLowPower) {
  BooleanMediumSelector booleanMediumSelector;
  booleanMediumSelector.bluetooth = false;

  AdvertisingOptions advertising_options{
      {
          strategy_,
          booleanMediumSelector,
      },
      false,  // auto_upgrade_bandwidth
      false,  // enforce_topology_constraints
      true,   // low_power
  };
  Endpoint advertising_endpoint_1 = StartAdvertising(
      &client1_, advertising_connection_listener_, advertising_options);

  StopAdvertising(&client1_);

  Endpoint advertising_endpoint_2 = StartAdvertising(
      &client1_, advertising_connection_listener_, advertising_options);

  EXPECT_NE(advertising_endpoint_1.id, advertising_endpoint_2.id);
}

TEST_F(ClientProxyTest, NotLogSessionForStoppedAdvertisingWithConnection) {
  Endpoint advertising_endpoint =
      StartAdvertising(&client1_, advertising_connection_listener_);
  OnAdvertisingConnectionInitiated(&client1_, advertising_endpoint);

  StartDiscovery(&client2_, GetDiscoveryListener());
  OnDiscoveryEndpointFound(&client2_, advertising_endpoint);
  OnDiscoveryConnectionInitiated(&client2_, advertising_endpoint);

  // Before
  EXPECT_TRUE(client1_.HasPendingConnectionToEndpoint(
      advertising_endpoint.id));           // Connections are available
  EXPECT_FALSE(client1_.IsDiscovering());  // No Discovery
  EXPECT_TRUE(client1_.IsAdvertising());   // Advertising

  // After
  StopAdvertising(&client1_);  // No Advertising
  client1_.GetAnalyticsRecorder().Sync();
  EXPECT_EQ(event_logger1_.GetCompleteClientSessionCount(), 0);
}

TEST_F(ClientProxyTest,
       LogSessionForStoppedAdvertisingWhenNoConnectionsAndNoDiscovering) {
  Endpoint advertising_endpoint =
      StartAdvertising(&client1_, advertising_connection_listener_);

  // Before
  EXPECT_FALSE(client1_.HasPendingConnectionToEndpoint(
      advertising_endpoint.id));           // No Connections
  EXPECT_FALSE(client1_.IsDiscovering());  // No Discovery
  EXPECT_TRUE(client1_.IsAdvertising());   // Advertising
  client1_.GetAnalyticsRecorder().Sync();
  EXPECT_EQ(event_logger1_.GetCompleteClientSessionCount(), 0);

  // After
  StopAdvertising(&client1_);
  client1_.GetAnalyticsRecorder().Sync();
  EXPECT_GT(event_logger1_.GetCompleteClientSessionCount(), 0);
}

TEST_F(ClientProxyTest, NotLogSessionForStoppedDiscoveryWithConnection) {
  Endpoint advertising_endpoint =
      StartAdvertising(&client1_, advertising_connection_listener_);

  StartDiscovery(&client2_, GetDiscoveryListener());
  OnDiscoveryEndpointFound(&client2_, advertising_endpoint);

  // Before
  OnDiscoveryConnectionInitiated(
      &client2_, advertising_endpoint);    // Connections are available
  EXPECT_FALSE(client2_.IsAdvertising());  // No Advertising
  EXPECT_TRUE(client2_.IsDiscovering());   // Discovering

  // After
  StopDiscovery(&client2_);
  client2_.GetAnalyticsRecorder().Sync();
  EXPECT_EQ(event_logger2_.GetCompleteClientSessionCount(), 0);
}

TEST_F(ClientProxyTest,
       NotLogSessionForStoppedDiscoveryWithoutConnectionsAndAdvertising) {
  Endpoint advertising_endpoint =
      StartAdvertising(&client1_, advertising_connection_listener_);

  StartDiscovery(&client2_, GetDiscoveryListener());

  // Before
  EXPECT_FALSE(client2_.IsAdvertising());  // No Advertising
  EXPECT_TRUE(client2_.IsDiscovering());   // Discoverying
  EXPECT_FALSE(client2_.HasPendingConnectionToEndpoint(
      advertising_endpoint.id));  // No Connections

  // After
  StopDiscovery(&client2_);
  client2_.GetAnalyticsRecorder().Sync();
  EXPECT_GT(event_logger2_.GetCompleteClientSessionCount(), 0);
}

TEST_F(ClientProxyTest, LogSessionOnDisconnectedWithOneConnection) {
  Endpoint advertising_endpoint =
      StartAdvertising(&client1_, advertising_connection_listener_);
  StartDiscovery(&client2_, GetDiscoveryListener());
  OnDiscoveryEndpointFound(&client2_, advertising_endpoint);
  OnDiscoveryConnectionInitiated(&client2_, advertising_endpoint);

  // Before
  EXPECT_FALSE(client2_.IsAdvertising());  // No Advertising
  StopDiscovery(&client2_);                // No Discovery
  EXPECT_TRUE(client2_.HasPendingConnectionToEndpoint(
      advertising_endpoint.id));  // One Connection

  // After
  OnDiscoveryConnectionDisconnected(&client2_, advertising_endpoint);
  client2_.GetAnalyticsRecorder().Sync();
  EXPECT_GT(event_logger2_.GetCompleteClientSessionCount(), 0);
}

TEST_F(ClientProxyTest,
       NotLogSessionOnDisconnectedWithoutConnectionsDiscoveringAdvertising) {
  Endpoint advertising_endpoint =
      StartAdvertising(&client1_, advertising_connection_listener_);

  // Before
  EXPECT_FALSE(client2_.IsAdvertising());  // No Advertising
  EXPECT_FALSE(client2_.IsDiscovering());  // No Discovery
  EXPECT_FALSE(client2_.HasPendingConnectionToEndpoint(
      advertising_endpoint.id));  // No Connections

  // After
  client2_.OnDisconnected(advertising_endpoint.id, /*notify=*/false);
  client2_.GetAnalyticsRecorder().Sync();
  EXPECT_EQ(event_logger2_.GetCompleteClientSessionCount(), 0);
}

TEST_F(ClientProxyTest, NotLogSessionOnDisconnectedWhenMoreThanOneConnection) {
  ClientProxy client3;
  Endpoint advertising_endpoint_1 =
      StartAdvertising(&client1_, advertising_connection_listener_);
  Endpoint advertising_endpoint_2 =
      StartAdvertising(&client2_, advertising_connection_listener_);
  StartDiscovery(&client3, GetDiscoveryListener());

  OnDiscoveryEndpointFound(&client3, advertising_endpoint_1);
  OnDiscoveryConnectionInitiated(&client3, advertising_endpoint_1);
  OnDiscoveryEndpointFound(&client3, advertising_endpoint_2);
  OnDiscoveryConnectionInitiated(&client3, advertising_endpoint_2);

  // Before
  // - More than one Connection
  EXPECT_TRUE(
      client3.HasPendingConnectionToEndpoint(advertising_endpoint_1.id));
  EXPECT_TRUE(
      client3.HasPendingConnectionToEndpoint(advertising_endpoint_2.id));
  EXPECT_FALSE(client3.IsAdvertising());  // No Advertising
  StopDiscovery(&client3);                // No Discovery

  // After
  client2_.OnDisconnected(advertising_endpoint_1.id, /*notify=*/false);
  client2_.GetAnalyticsRecorder().Sync();
  EXPECT_EQ(event_logger2_.GetCompleteClientSessionCount(), 0);
}

TEST_F(ClientProxyTest,
       NotLogSessionOnDisconnectedForDiscoveringWithOnlyOneConnection) {
  Endpoint advertising_endpoint =
      StartAdvertising(&client1_, advertising_connection_listener_);
  StartDiscovery(&client2_, GetDiscoveryListener());
  OnDiscoveryEndpointFound(&client2_, advertising_endpoint);
  OnDiscoveryConnectionInitiated(&client2_, advertising_endpoint);

  // Before
  EXPECT_FALSE(client2_.IsAdvertising());  // No Advertising
  EXPECT_TRUE(client2_.IsDiscovering());   // Discovering
  EXPECT_TRUE(client2_.HasPendingConnectionToEndpoint(
      advertising_endpoint.id));  // One Connection

  // After
  OnDiscoveryConnectionDisconnected(&client2_, advertising_endpoint);
  client2_.GetAnalyticsRecorder().Sync();
  EXPECT_EQ(event_logger2_.GetCompleteClientSessionCount(), 0);
}

TEST_F(ClientProxyTest, LogSessionForResetClientProxy) {
  Endpoint advertising_endpoint =
      StartAdvertising(&client1_, advertising_connection_listener_);
  StartDiscovery(&client2_, GetDiscoveryListener());
  OnDiscoveryEndpointFound(&client2_, advertising_endpoint);
  OnDiscoveryConnectionInitiated(&client2_, advertising_endpoint);

  client1_.GetAnalyticsRecorder().Sync();
  EXPECT_EQ(event_logger1_.GetCompleteClientSessionCount(), 0);
  client1_.Reset();
  client1_.GetAnalyticsRecorder().Sync();
  // TODO(b/290936886): Why are there more than one complete sessions?
  EXPECT_GT(event_logger1_.GetCompleteClientSessionCount(), 0);

  client2_.GetAnalyticsRecorder().Sync();
  EXPECT_EQ(event_logger2_.GetCompleteClientSessionCount(), 0);
  client2_.Reset();
  client2_.GetAnalyticsRecorder().Sync();
  EXPECT_GT(event_logger2_.GetCompleteClientSessionCount(), 0);
}

TEST_F(ClientProxyTest, GetLocalInfoCorrect) {
  ClientProxy client;
  // Default is g3 test Environment as LINUX.
  EXPECT_EQ(client.GetLocalOsInfo().type(), OsInfo::LINUX);
}

TEST_F(ClientProxyTest, GetRemoteInfoNullWithoutConnections) {
  Endpoint advertising_endpoint =
      StartAdvertising(&client1_, advertising_connection_listener_);

  EXPECT_FALSE(client1_.GetRemoteOsInfo(advertising_endpoint.id).has_value());
  EXPECT_FALSE(
      client1_.GetRemoteSafeToDisconnectVersion(advertising_endpoint.id)
          .has_value());
}

TEST_F(ClientProxyTest, SetRemoteInfoCorrect) {
  Endpoint advertising_endpoint =
      StartAdvertising(&client1_, advertising_connection_listener_);
  OnAdvertisingConnectionInitiated(&client1_, advertising_endpoint);

  OsInfo os_info;
  os_info.set_type(OsInfo::ANDROID);
  std::int32_t nearby_connections_version = 2;
  client1_.SetRemoteOsInfo(advertising_endpoint.id, os_info);
  client1_.SetRemoteSafeToDisconnectVersion(advertising_endpoint.id,
                                            nearby_connections_version);

  ASSERT_TRUE(client1_.GetRemoteOsInfo(advertising_endpoint.id).has_value());
  EXPECT_EQ(client1_.GetRemoteOsInfo(advertising_endpoint.id).value().type(),
            OsInfo::ANDROID);
  EXPECT_EQ(client1_.GetRemoteSafeToDisconnectVersion(advertising_endpoint.id),
            nearby_connections_version);
}

// Test ClientProxy::AddCancellationFlag, where if a flag is already in the map,
// uncancel it. This addresses the case when users use NS to share/receive a
// file, then cancel in the middle because the wrong file was selected, and then
// re-do right after. Without the ability to uncancel a flag in
// `AddCancelationFlag`, the second share/receive process will
// be seen as cancelled with cancellation flags enabled. However this tests that
// it will uncancel the flag which is added in RequestConnection and
// OnConnectionInitiated in the NS flow, and allow another attempt with the
// same endpoint.
TEST_F(ClientProxyTest, UncancelCancellationFlags) {
  // Enable cancellation flags.
  MediumEnvironment::Instance().SetFeatureFlags(kTestCases[0]);
  Endpoint advertising_endpoint =
      StartAdvertising(&client1_, advertising_connection_listener_);

  // Add a cancellation flag to the client proxy.
  client1_.AddCancellationFlag(advertising_endpoint.id);
  auto flag = client1_.GetCancellationFlag(advertising_endpoint.id);
  EXPECT_FALSE(flag->Cancelled());
  EXPECT_FALSE(
      client1_.GetCancellationFlag(advertising_endpoint.id)->Cancelled());

  // Cancel the flag.
  flag->Cancel();
  EXPECT_TRUE(flag->Cancelled());
  EXPECT_TRUE(
      client1_.GetCancellationFlag(advertising_endpoint.id)->Cancelled());

  // On subsequent calls to add a new cancellation flag, expect an the flag to
  // be uncancelled.
  client1_.AddCancellationFlag(advertising_endpoint.id);
  flag = client1_.GetCancellationFlag(advertising_endpoint.id);
  EXPECT_FALSE(flag->Cancelled());
  EXPECT_FALSE(
      client1_.GetCancellationFlag(advertising_endpoint.id)->Cancelled());
}

TEST_F(ClientProxyTest, GetLocalDeviceWorksWithoutDeviceProvider) {
  auto device = client1_.GetLocalDevice();
  EXPECT_NE(device, nullptr);
  EXPECT_EQ(device->GetEndpointId().length(), 4);
  EXPECT_NE(client1_.GetLocalDeviceProvider(), nullptr);
}

TEST_F(ClientProxyTest, GetLocalDeviceWorksWithDeviceProvider) {
  MockDeviceProvider provider;
  client1_.RegisterDeviceProvider(&provider);
  ASSERT_NE(client1_.GetLocalDeviceProvider(), nullptr);
  EXPECT_CALL(
      *(down_cast<MockDeviceProvider*>(client1_.GetLocalDeviceProvider())),
      GetLocalDevice);
  client1_.GetLocalDevice();
}

TEST_F(ClientProxyTest, TestGetSetLocalEndpointInfo) {
  client1_.UpdateLocalEndpointInfo("endpoint_info");
  EXPECT_EQ(client1_.GetLocalEndpointInfo(), "endpoint_info");
}

TEST_F(ClientProxyTest, TestGetIncomingConnectionListener) {
  CountDownLatch result_latch(2);
  CountDownLatch bwu_latch(1);
  CountDownLatch disconnect_latch(1);
  CountDownLatch init_latch(1);
  client1_.StartedListeningForIncomingConnections(
      service_id_, Strategy::kP2pCluster,
      {
          .initiated_cb =
              [&init_latch](const NearbyDevice&,
                            const v3::InitialConnectionInfo&) {
                init_latch.CountDown();
              },
          .result_cb = [&result_latch](
                           const NearbyDevice&,
                           v3::ConnectionResult) { result_latch.CountDown(); },
          .disconnected_cb =
              [&disconnect_latch](const NearbyDevice&) {
                disconnect_latch.CountDown();
              },
          .bandwidth_changed_cb =
              [&bwu_latch](const NearbyDevice&, v3::BandwidthInfo) {
                bwu_latch.CountDown();
              },
      },
      {});
  auto listener = client1_.GetAdvertisingOrIncomingConnectionListener();
  listener.accepted_cb("endpoint-id");
  listener.initiated_cb("endpoint-id", {.is_incoming_connection = false});
  listener.disconnected_cb("endpoint-id");
  listener.rejected_cb("endpoint-id", {Status::Value::kConnectionRejected});
  listener.bandwidth_changed_cb("endpoint-id", Medium::WIFI_LAN);
  EXPECT_TRUE(result_latch.Await().Ok());
  EXPECT_TRUE(init_latch.Await().Ok());
  EXPECT_TRUE(bwu_latch.Await().Ok());
  EXPECT_TRUE(disconnect_latch.Await().Ok());
}

TEST_F(ClientProxyTest, EnforceTopologyWhenRequestedAdvertising) {
  EXPECT_FALSE(client1_.ShouldEnforceTopologyConstraints());
  StartAdvertising(&client1_, advertising_connection_listener_,
                   {.enforce_topology_constraints = true});
  EXPECT_TRUE(client1_.ShouldEnforceTopologyConstraints());
}

TEST_F(ClientProxyTest, EnforceTopologyWhenRequestedListeningWithStrategy) {
  EXPECT_FALSE(client1_.ShouldEnforceTopologyConstraints());
  StartListeningForIncomingConnections(&client1_, {},
                                       {.strategy = Strategy::kP2pCluster,
                                        .enforce_topology_constraints = true});
  EXPECT_TRUE(client1_.ShouldEnforceTopologyConstraints());
}

TEST_F(ClientProxyTest, DontEnforceTopologyWhenRequestedWithNoStrategy) {
  EXPECT_FALSE(client1_.ShouldEnforceTopologyConstraints());
  StartListeningForIncomingConnections(&client1_, {},
                                       {.strategy = Strategy::kNone});
  EXPECT_TRUE(client1_.ShouldEnforceTopologyConstraints());
}

TEST_F(ClientProxyTest, TestAutoBwuWhenAdvertisingWithAutoBwu) {
  EXPECT_FALSE(client1_.AutoUpgradeBandwidth());
  StartAdvertising(&client1_, advertising_connection_listener_,
                   {.auto_upgrade_bandwidth = true});
  EXPECT_TRUE(client1_.AutoUpgradeBandwidth());
}

TEST_F(ClientProxyTest, TestAutoBwuWhenListeningWithAutoBwu) {
  EXPECT_FALSE(client1_.AutoUpgradeBandwidth());
  StartListeningForIncomingConnections(&client1_, {},
                                       {.auto_upgrade_bandwidth = true});
  EXPECT_TRUE(client1_.AutoUpgradeBandwidth());
}

}  // namespace
}  // namespace connections
}  // namespace nearby
