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

#include "core_v2/internal/client_proxy.h"

#include <string>

#include "core_v2/listeners.h"
#include "core_v2/options.h"
#include "core_v2/strategy.h"
#include "platform_v2/base/byte_array.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/container/flat_hash_set.h"
#include "absl/types/span.h"

namespace location {
namespace nearby {
namespace connections {
namespace {

using ::testing::MockFunction;
using ::testing::StrictMock;

class ClientProxyTest : public testing::Test {
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

  Endpoint StartAdvertising(ClientProxy* client, ConnectionListener listener) {
    Endpoint endpoint{
        .info = ByteArray{"advertising endpoint name"},
        .id = client->GetLocalEndpointId(),
    };
    client->StartedAdvertising(service_id_, strategy_, listener,
                               absl::MakeSpan(mediums_));
    return endpoint;
  }

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
    client->OnEndpointFound(service_id_, endpoint.id, endpoint.info,
                            medium_);
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

TEST_F(ClientProxyTest, ConstructorDestructorWorks) { SUCCEED(); }

TEST_F(ClientProxyTest, ClientIdIsUnique) {
  EXPECT_NE(client1_.GetClientId(), client2_.GetClientId());
}

TEST_F(ClientProxyTest, GeneratedEndpointIdIsUnique) {
  EXPECT_NE(client1_.GetLocalEndpointId(),
            client2_.GetLocalEndpointId());
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

}  // namespace
}  // namespace connections
}  // namespace nearby
}  // namespace location
