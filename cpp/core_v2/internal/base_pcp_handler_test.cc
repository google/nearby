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

#include "core_v2/internal/base_pcp_handler.h"

#include <atomic>
#include <memory>

#include "core_v2/internal/base_endpoint_channel.h"
#include "core_v2/internal/bwu_manager.h"
#include "core_v2/internal/client_proxy.h"
#include "core_v2/internal/encryption_runner.h"
#include "core_v2/internal/offline_frames.h"
#include "core_v2/listeners.h"
#include "core_v2/options.h"
#include "core_v2/params.h"
#include "proto/connections/offline_wire_formats.pb.h"
#include "platform_v2/base/byte_array.h"
#include "platform_v2/public/count_down_latch.h"
#include "platform_v2/public/pipe.h"
#include "proto/connections_enums.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/time/time.h"

namespace location {
namespace nearby {
namespace connections {
namespace {

using ::location::nearby::proto::connections::Medium;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::Invoke;
using ::testing::MockFunction;
using ::testing::Return;
using ::testing::StrictMock;

constexpr BooleanMediumSelector kTestCases[] = {
    BooleanMediumSelector{},
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

class MockEndpointChannel : public BaseEndpointChannel {
 public:
  explicit MockEndpointChannel(Pipe* reader, Pipe* writer)
      : BaseEndpointChannel("channel", &reader->GetInputStream(),
                            &writer->GetOutputStream()) {}

  ExceptionOr<ByteArray> DoRead() { return BaseEndpointChannel::Read(); }
  Exception DoWrite(const ByteArray& data) {
    return BaseEndpointChannel::Write(data);
  }
  absl::Time DoGetLastReadTimestamp() {
    return BaseEndpointChannel::GetLastReadTimestamp();
  }

  MOCK_METHOD(ExceptionOr<ByteArray>, Read, (), (override));
  MOCK_METHOD(Exception, Write, (const ByteArray& data), (override));
  MOCK_METHOD(void, CloseImpl, (), (override));
  MOCK_METHOD(proto::connections::Medium, GetMedium, (), (const override));
  MOCK_METHOD(std::string, GetType, (), (const override));
  MOCK_METHOD(std::string, GetName, (), (const override));
  MOCK_METHOD(bool, IsPaused, (), (const override));
  MOCK_METHOD(void, Pause, (), (override));
  MOCK_METHOD(void, Resume, (), (override));
  MOCK_METHOD(absl::Time, GetLastReadTimestamp, (), (const override));
};

class MockPcpHandler : public BasePcpHandler {
 public:
  using DiscoveredEndpoint = BasePcpHandler::DiscoveredEndpoint;

  MockPcpHandler(Mediums* m, EndpointManager* em, EndpointChannelManager* ecm,
                 BwuManager* bwu)
      : BasePcpHandler(m, em, ecm, bwu, Pcp::kP2pCluster) {}

  // Expose protected inner types of a base type for mocking.
  using BasePcpHandler::ConnectImplResult;
  using BasePcpHandler::DiscoveredEndpoint;
  using BasePcpHandler::StartOperationResult;

  MOCK_METHOD(Strategy, GetStrategy, (), (const override));
  MOCK_METHOD(Pcp, GetPcp, (), (const override));

  MOCK_METHOD(bool, HasOutgoingConnections, (ClientProxy * client),
              (const, override));
  MOCK_METHOD(bool, HasIncomingConnections, (ClientProxy * client),
              (const, override));

  MOCK_METHOD(bool, CanSendOutgoingConnection, (ClientProxy * client),
              (const, override));
  MOCK_METHOD(bool, CanReceiveIncomingConnection, (ClientProxy * client),
              (const, override));

  MOCK_METHOD(StartOperationResult, StartAdvertisingImpl,
              (ClientProxy * client, const std::string& service_id,
               const std::string& local_endpoint_id,
               const ByteArray& local_endpoint_info,
               const ConnectionOptions& options),
              (override));
  MOCK_METHOD(Status, StopAdvertisingImpl, (ClientProxy * client), (override));
  MOCK_METHOD(StartOperationResult, StartDiscoveryImpl,
              (ClientProxy * client, const string& service_id,
               const ConnectionOptions& options),
              (override));
  MOCK_METHOD(Status, StopDiscoveryImpl, (ClientProxy * client), (override));
  MOCK_METHOD(ConnectImplResult, ConnectImpl,
              (ClientProxy * client, DiscoveredEndpoint* endpoint), (override));
  MOCK_METHOD(proto::connections::Medium, GetDefaultUpgradeMedium, (),
              (override));

  std::vector<proto::connections::Medium> GetConnectionMediumsByPriority()
      override {
    return GetDiscoveryMediums();
  }

  // Mock adapters for protected non-virtual methods of a base class.
  void OnEndpointFound(ClientProxy* client,
                       std::shared_ptr<DiscoveredEndpoint> endpoint) {
    BasePcpHandler::OnEndpointFound(client, std::move(endpoint));
  }
  void OnEndpointLost(ClientProxy* client, const DiscoveredEndpoint& endpoint) {
    BasePcpHandler::OnEndpointLost(client, endpoint);
  }
  std::vector<BasePcpHandler::DiscoveredEndpoint*> GetDiscoveredEndpoints(
      const std::string& endpoint_id) {
    return BasePcpHandler::GetDiscoveredEndpoints(endpoint_id);
  }

  std::vector<proto::connections::Medium> GetDiscoveryMediums() {
    auto allowed =
        BasePcpHandler::GetDiscoveryOptions().CompatibleOptions().allowed;
    return GetMediumsFromSelector(allowed);
  }

  std::vector<proto::connections::Medium> GetMediumsFromSelector(
      BooleanMediumSelector allowed) {
    return allowed.GetMediums(true);
  }
};

class MockContext {
 public:
  explicit MockContext(std::atomic_int* destroyed = nullptr) {
    destroyed_ = destroyed;
  }
  MockContext(MockContext&&) = default;
  MockContext& operator=(MockContext&&) = default;

  ~MockContext() {
    if (destroyed_) (*destroyed_)++;
  }

 private:
  Swapper<std::atomic_int> destroyed_{nullptr};
};

struct MockDiscoveredEndpoint : public MockPcpHandler::DiscoveredEndpoint {
  MockDiscoveredEndpoint(DiscoveredEndpoint endpoint, MockContext context)
      : DiscoveredEndpoint(std::move(endpoint)), context(std::move(context)) {}

  MockContext context;
};

class BasePcpHandlerTest
    : public ::testing::TestWithParam<BooleanMediumSelector> {
 protected:
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
  struct MockDiscoveryListener {
    StrictMock<MockFunction<void(const std::string& endpoint_id,
                                 const ByteArray& endpoint_info,
                                 const std::string& service_id)>>
        endpoint_found_cb;
    StrictMock<MockFunction<void(const std::string& endpoint_id)>>
        endpoint_lost_cb;
    StrictMock<
        MockFunction<void(const std::string& endpoint_id, DistanceInfo info)>>
        endpoint_distance_changed_cb;
  };

  void StartAdvertising(ClientProxy* client, MockPcpHandler* pcp_handler,
                        BooleanMediumSelector allowed = GetParam()) {
    std::string service_id{"service"};
    ConnectionOptions options{
        .strategy = Strategy::kP2pCluster,
        .allowed = allowed,
        .auto_upgrade_bandwidth = true,
        .enforce_topology_constraints = true,
    };
    ConnectionRequestInfo info{
        .endpoint_info = ByteArray{"remote_endpoint_name"},
        .listener = connection_listener_,
    };
    EXPECT_CALL(*pcp_handler, StartAdvertisingImpl(client, service_id, _,
                                                   info.endpoint_info, _))
        .WillOnce(Return(MockPcpHandler::StartOperationResult{
            .status = {Status::kSuccess},
            .mediums = pcp_handler->GetMediumsFromSelector(allowed),
        }));
    EXPECT_EQ(pcp_handler->StartAdvertising(client, service_id, options, info),
              Status{Status::kSuccess});
    EXPECT_TRUE(client->IsAdvertising());
  }

  void StartDiscovery(ClientProxy* client, MockPcpHandler* pcp_handler,
                      BooleanMediumSelector allowed = GetParam()) {
    std::string service_id{"service"};
    ConnectionOptions options{
        .strategy = Strategy::kP2pCluster,
        .allowed = allowed,
        .auto_upgrade_bandwidth = true,
        .enforce_topology_constraints = true,
    };
    EXPECT_CALL(*pcp_handler, StartDiscoveryImpl(client, service_id, _))
        .WillOnce(Return(MockPcpHandler::StartOperationResult{
            .status = {Status::kSuccess},
            .mediums = pcp_handler->GetMediumsFromSelector(allowed),
        }));
    EXPECT_EQ(pcp_handler->StartDiscovery(client, service_id, options,
                                          discovery_listener_),
              Status{Status::kSuccess});
    EXPECT_TRUE(client->IsDiscovering());
  }

  std::pair<std::unique_ptr<MockEndpointChannel>,
            std::unique_ptr<MockEndpointChannel>>
  SetupConnection(Pipe& pipe_a, Pipe& pipe_b,
                  proto::connections::Medium medium) {  // NOLINT
    auto channel_a = std::make_unique<MockEndpointChannel>(&pipe_b, &pipe_a);
    auto channel_b = std::make_unique<MockEndpointChannel>(&pipe_a, &pipe_b);
    // On initiator (A) side, we drop the first write, since this is a
    // connection establishment packet, and we don't have the peer entity, just
    // the peer channel. The rest of the exchange must happen for the benefit of
    // DH key exchange.
    EXPECT_CALL(*channel_a, Read())
        .WillRepeatedly(Invoke(
            [channel = channel_a.get()]() { return channel->DoRead(); }));
    EXPECT_CALL(*channel_a, Write(_))
        .WillOnce(Return(Exception{Exception::kSuccess}))
        .WillRepeatedly(
            Invoke([channel = channel_a.get()](const ByteArray& data) {
              return channel->DoWrite(data);
            }));
    EXPECT_CALL(*channel_a, GetMedium).WillRepeatedly(Return(medium));
    EXPECT_CALL(*channel_a, GetLastReadTimestamp)
        .WillRepeatedly(Return(absl::Now()));
    EXPECT_CALL(*channel_a, IsPaused).WillRepeatedly(Return(false));
    EXPECT_CALL(*channel_b, Read())
        .WillRepeatedly(Invoke(
            [channel = channel_b.get()]() { return channel->DoRead(); }));
    EXPECT_CALL(*channel_b, Write(_))
        .WillRepeatedly(
            Invoke([channel = channel_b.get()](const ByteArray& data) {
              return channel->DoWrite(data);
            }));
    EXPECT_CALL(*channel_b, GetMedium).WillRepeatedly(Return(medium));
    EXPECT_CALL(*channel_b, GetLastReadTimestamp)
        .WillRepeatedly(Return(absl::Now()));
    EXPECT_CALL(*channel_b, IsPaused).WillRepeatedly(Return(false));
    return std::make_pair(std::move(channel_a), std::move(channel_b));
  }

  void RequestConnection(const std::string& endpoint_id,
                         std::unique_ptr<MockEndpointChannel> channel_a,
                         MockEndpointChannel* channel_b, ClientProxy* client,
                         MockPcpHandler* pcp_handler,
                         proto::connections::Medium connect_medium,
                         std::atomic_int* flag = nullptr) {
    ConnectionRequestInfo info{
        .endpoint_info = ByteArray{"ABCD"},
        .listener = connection_listener_,
    };
    ConnectionOptions options{
        .remote_bluetooth_mac_address =
            ByteArray{std::string("\x12\x34\x56\x78\x9a\xbc")},
    };
    EXPECT_CALL(mock_discovery_listener_.endpoint_found_cb, Call);
    EXPECT_CALL(*pcp_handler, CanSendOutgoingConnection)
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*pcp_handler, GetStrategy)
        .WillRepeatedly(Return(Strategy::kP2pCluster));
    EXPECT_CALL(mock_connection_listener_.initiated_cb, Call).Times(1);
    // Simulate successful discovery.
    auto encryption_runner = std::make_unique<EncryptionRunner>();
    auto allowed_mediums = pcp_handler->GetDiscoveryMediums();

    EXPECT_CALL(*pcp_handler, ConnectImpl)
        .WillOnce(Invoke([&channel_a, connect_medium](
                             ClientProxy* client,
                             MockPcpHandler::DiscoveredEndpoint* endpoint) {
          return MockPcpHandler::ConnectImplResult{
              .medium = connect_medium,
              .status = {Status::kSuccess},
              .endpoint_channel = std::move(channel_a),
          };
        }));

    for (const auto& discovered_medium : allowed_mediums) {
      pcp_handler->OnEndpointFound(
          client,
          std::make_shared<MockDiscoveredEndpoint>(MockDiscoveredEndpoint{
              {
                  endpoint_id,
                  info.endpoint_info,
                  "service",
                  discovered_medium,
              },
              MockContext{flag},
          }));
    }
    auto other_client = std::make_unique<ClientProxy>();

    // Run peer crypto in advance, if channel_b is provided.
    // Otherwise stay in not-encrypted state.
    if (channel_b != nullptr) {
      encryption_runner->StartServer(other_client.get(), endpoint_id, channel_b,
                                     {});
    }
    EXPECT_EQ(
        pcp_handler->RequestConnection(client, endpoint_id, info, options),
        Status{Status::kSuccess});
    NEARBY_LOG(INFO, "Stopping Encryption Runner");
  }

  Pipe pipe_a_;
  Pipe pipe_b_;
  MockConnectionListener mock_connection_listener_;
  MockDiscoveryListener mock_discovery_listener_;
  ConnectionListener connection_listener_{
      .initiated_cb = mock_connection_listener_.initiated_cb.AsStdFunction(),
      .accepted_cb = mock_connection_listener_.accepted_cb.AsStdFunction(),
      .rejected_cb = mock_connection_listener_.rejected_cb.AsStdFunction(),
      .disconnected_cb =
          mock_connection_listener_.disconnected_cb.AsStdFunction(),
      .bandwidth_changed_cb =
          mock_connection_listener_.bandwidth_changed_cb.AsStdFunction(),
  };
  DiscoveryListener discovery_listener_{
      .endpoint_found_cb =
          mock_discovery_listener_.endpoint_found_cb.AsStdFunction(),
      .endpoint_lost_cb =
          mock_discovery_listener_.endpoint_lost_cb.AsStdFunction(),
      .endpoint_distance_changed_cb =
          mock_discovery_listener_.endpoint_distance_changed_cb.AsStdFunction(),
  };
};

TEST_P(BasePcpHandlerTest, ConstructorDestructorWorks) {
  Mediums m;
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  BwuManager bwu(m, em, ecm, {}, {});
  MockPcpHandler pcp_handler(&m, &em, &ecm, &bwu);
  SUCCEED();
}

TEST_P(BasePcpHandlerTest, StartAdvertisingChangesState) {
  ClientProxy client;
  Mediums m;
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  BwuManager bwu(m, em, ecm, {}, {});
  MockPcpHandler pcp_handler(&m, &em, &ecm, &bwu);
  StartAdvertising(&client, &pcp_handler);
}

TEST_P(BasePcpHandlerTest, StopAdvertisingChangesState) {
  ClientProxy client;
  Mediums m;
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  BwuManager bwu(m, em, ecm, {}, {});
  MockPcpHandler pcp_handler(&m, &em, &ecm, &bwu);
  StartAdvertising(&client, &pcp_handler);
  EXPECT_CALL(pcp_handler, StopAdvertisingImpl(&client)).Times(1);
  EXPECT_TRUE(client.IsAdvertising());
  pcp_handler.StopAdvertising(&client);
  EXPECT_FALSE(client.IsAdvertising());
}

TEST_P(BasePcpHandlerTest, StartDiscoveryChangesState) {
  ClientProxy client;
  Mediums m;
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  BwuManager bwu(m, em, ecm, {}, {});
  MockPcpHandler pcp_handler(&m, &em, &ecm, &bwu);
  StartDiscovery(&client, &pcp_handler);
}

TEST_P(BasePcpHandlerTest, StopDiscoveryChangesState) {
  ClientProxy client;
  Mediums m;
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  BwuManager bwu(m, em, ecm, {}, {});
  MockPcpHandler pcp_handler(&m, &em, &ecm, &bwu);
  StartDiscovery(&client, &pcp_handler);
  EXPECT_CALL(pcp_handler, StopDiscoveryImpl(&client)).Times(1);
  EXPECT_TRUE(client.IsDiscovering());
  pcp_handler.StopDiscovery(&client);
  EXPECT_FALSE(client.IsDiscovering());
}

TEST_P(BasePcpHandlerTest, RequestConnectionChangesState) {
  std::string endpoint_id{"1234"};
  ClientProxy client;
  Mediums m;
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  BwuManager bwu(m, em, ecm, {}, {});
  MockPcpHandler pcp_handler(&m, &em, &ecm, &bwu);
  StartDiscovery(&client, &pcp_handler);
  auto mediums = pcp_handler.GetDiscoveryMediums();
  auto connect_medium = mediums[mediums.size() - 1];
  auto channel_pair = SetupConnection(pipe_a_, pipe_b_, connect_medium);
  auto& channel_a = channel_pair.first;
  auto& channel_b = channel_pair.second;
  EXPECT_CALL(*channel_a, CloseImpl).Times(1);
  EXPECT_CALL(*channel_b, CloseImpl).Times(1);
  EXPECT_CALL(mock_connection_listener_.rejected_cb, Call).Times(AtLeast(0));
  RequestConnection(endpoint_id, std::move(channel_a), channel_b.get(), &client,
                    &pcp_handler, connect_medium);
  NEARBY_LOG(INFO, "RequestConnection complete");
  channel_b->Close();
  pcp_handler.DisconnectFromEndpointManager();
}

TEST_P(BasePcpHandlerTest, AcceptConnectionChangesState) {
  std::string endpoint_id{"1234"};
  ClientProxy client;
  Mediums m;
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  BwuManager bwu(m, em, ecm, {}, {});
  MockPcpHandler pcp_handler(&m, &em, &ecm, &bwu);
  StartDiscovery(&client, &pcp_handler);
  auto mediums = pcp_handler.GetDiscoveryMediums();
  auto connect_medium = mediums[mediums.size() - 1];
  auto channel_pair = SetupConnection(pipe_a_, pipe_b_, connect_medium);
  auto& channel_a = channel_pair.first;
  auto& channel_b = channel_pair.second;
  EXPECT_CALL(*channel_a, CloseImpl).Times(1);
  EXPECT_CALL(*channel_b, CloseImpl).Times(1);
  RequestConnection(endpoint_id, std::move(channel_a), channel_b.get(), &client,
                    &pcp_handler, connect_medium);
  NEARBY_LOG(INFO, "Attempting to accept connection: id=%s",
             endpoint_id.c_str());
  EXPECT_EQ(pcp_handler.AcceptConnection(&client, endpoint_id, {}),
            Status{Status::kSuccess});
  EXPECT_CALL(mock_connection_listener_.rejected_cb, Call).Times(AtLeast(0));
  NEARBY_LOGS(INFO) << "Closing connection: id=" << endpoint_id;
  channel_b->Close();
  pcp_handler.DisconnectFromEndpointManager();
}

TEST_P(BasePcpHandlerTest, RejectConnectionChangesState) {
  std::string endpoint_id{"1234"};
  ClientProxy client;
  Mediums m;
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  BwuManager bwu(m, em, ecm, {}, {});
  MockPcpHandler pcp_handler(&m, &em, &ecm, &bwu);
  StartDiscovery(&client, &pcp_handler);
  auto mediums = pcp_handler.GetDiscoveryMediums();
  auto connect_medium = mediums[mediums.size() - 1];
  auto channel_pair = SetupConnection(pipe_a_, pipe_b_, connect_medium);
  auto& channel_b = channel_pair.second;
  EXPECT_CALL(mock_connection_listener_.rejected_cb, Call).Times(1);
  RequestConnection(endpoint_id, std::move(channel_pair.first), channel_b.get(),
                    &client, &pcp_handler, connect_medium);
  NEARBY_LOGS(INFO) << "Attempting to reject connection: id=" << endpoint_id;
  EXPECT_EQ(pcp_handler.RejectConnection(&client, endpoint_id),
            Status{Status::kSuccess});
  NEARBY_LOGS(INFO) << "Closing connection: id=" << endpoint_id;
  channel_b->Close();
  pcp_handler.DisconnectFromEndpointManager();
}

TEST_P(BasePcpHandlerTest, OnIncomingFrameChangesState) {
  std::string endpoint_id{"1234"};
  ClientProxy client;
  Mediums m;
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  BwuManager bwu(m, em, ecm, {}, {});
  MockPcpHandler pcp_handler(&m, &em, &ecm, &bwu);
  StartDiscovery(&client, &pcp_handler);
  auto mediums = pcp_handler.GetDiscoveryMediums();
  auto connect_medium = mediums[mediums.size() - 1];
  auto channel_pair = SetupConnection(pipe_a_, pipe_b_, connect_medium);
  auto& channel_a = channel_pair.first;
  auto& channel_b = channel_pair.second;
  EXPECT_CALL(*channel_a, CloseImpl).Times(1);
  EXPECT_CALL(*channel_b, CloseImpl).Times(1);
  RequestConnection(endpoint_id, std::move(channel_a), channel_b.get(), &client,
                    &pcp_handler, connect_medium);
  NEARBY_LOGS(INFO) << "Attempting to accept connection: id=" << endpoint_id;
  EXPECT_CALL(mock_connection_listener_.accepted_cb, Call).Times(1);
  EXPECT_CALL(mock_connection_listener_.disconnected_cb, Call)
      .Times(AtLeast(0));
  EXPECT_EQ(pcp_handler.AcceptConnection(&client, endpoint_id, {}),
            Status{Status::kSuccess});
  NEARBY_LOG(INFO, "Simulating remote accept: id=%s", endpoint_id.c_str());
  auto frame =
      parser::FromBytes(parser::ForConnectionResponse(Status::kSuccess));
  pcp_handler.OnIncomingFrame(frame.result(), endpoint_id, &client,
                              connect_medium);
  NEARBY_LOGS(INFO) << "Closing connection: id=" << endpoint_id;
  channel_b->Close();
  pcp_handler.DisconnectFromEndpointManager();
}

TEST_P(BasePcpHandlerTest, DestructorIsCalledOnProtocolEndpoint) {
  std::atomic_int destroyed_flag = 0;
  int mediums_count = 0;
  {
    std::string endpoint_id{"1234"};
    ClientProxy client;
    Mediums m;
    EndpointChannelManager ecm;
    EndpointManager em(&ecm);
    BwuManager bwu(m, em, ecm, {}, {});
    MockPcpHandler pcp_handler(&m, &em, &ecm, &bwu);
    StartDiscovery(&client, &pcp_handler);
    auto mediums = pcp_handler.GetDiscoveryMediums();
    auto connect_medium = mediums[mediums.size() - 1];
    auto channel_pair = SetupConnection(pipe_a_, pipe_b_, connect_medium);
    auto& channel_a = channel_pair.first;
    auto& channel_b = channel_pair.second;
    EXPECT_CALL(*channel_a, CloseImpl).Times(1);
    EXPECT_CALL(*channel_b, CloseImpl).Times(1);
    RequestConnection(endpoint_id, std::move(channel_a), channel_b.get(),
                      &client, &pcp_handler, connect_medium, &destroyed_flag);
    mediums_count = mediums.size();
    NEARBY_LOG(INFO, "Attempting to accept connection: id=%s",
               endpoint_id.c_str());
    EXPECT_EQ(pcp_handler.AcceptConnection(&client, endpoint_id, {}),
              Status{Status::kSuccess});
    EXPECT_CALL(mock_connection_listener_.rejected_cb, Call).Times(AtLeast(0));
    NEARBY_LOG(INFO, "Closing connection: id=%s", endpoint_id.c_str());
    channel_b->Close();
    pcp_handler.DisconnectFromEndpointManager();
  }
  EXPECT_EQ(destroyed_flag.load(), mediums_count);
}

TEST_P(BasePcpHandlerTest, MultipleMediumsProduceSingleEndpointLostEvent) {
  BooleanMediumSelector allowed = GetParam();
  if (allowed.Count(true) < 2) {
    // Ignore single-medium test cases, and implicit "all mediums" case.
    SUCCEED();
    return;
  }
  std::atomic_int destroyed_flag = 0;
  int mediums_count = 0;
  {
    std::string endpoint_id{"1234"};
    ClientProxy client;
    Mediums m;
    EndpointChannelManager ecm;
    EndpointManager em(&ecm);
    BwuManager bwu(m, em, ecm, {}, {});
    MockPcpHandler pcp_handler(&m, &em, &ecm, &bwu);
    StartDiscovery(&client, &pcp_handler);
    auto mediums = pcp_handler.GetDiscoveryMediums();
    auto connect_medium = mediums[mediums.size() - 1];
    auto channel_pair = SetupConnection(pipe_a_, pipe_b_, connect_medium);
    auto& channel_a = channel_pair.first;
    auto& channel_b = channel_pair.second;
    EXPECT_CALL(*channel_a, CloseImpl).Times(1);
    EXPECT_CALL(*channel_b, CloseImpl).Times(1);
    EXPECT_CALL(mock_discovery_listener_.endpoint_lost_cb, Call).Times(1);
    RequestConnection(endpoint_id, std::move(channel_a), channel_b.get(),
                      &client, &pcp_handler, connect_medium, &destroyed_flag);
    auto allowed_mediums = pcp_handler.GetDiscoveryMediums();
    mediums_count = allowed_mediums.size();
    NEARBY_LOG(INFO, "Attempting to accept connection: id=%s",
               endpoint_id.c_str());
    EXPECT_EQ(pcp_handler.AcceptConnection(&client, endpoint_id, {}),
              Status{Status::kSuccess});
    EXPECT_CALL(mock_connection_listener_.rejected_cb, Call).Times(AtLeast(0));
    for (const auto* endpoint :
         pcp_handler.GetDiscoveredEndpoints(endpoint_id)) {
      pcp_handler.OnEndpointLost(&client, *endpoint);
    }
    NEARBY_LOG(INFO, "Closing connection: id=%s", endpoint_id.c_str());
    channel_b->Close();
    pcp_handler.DisconnectFromEndpointManager();
  }
  EXPECT_EQ(destroyed_flag.load(), mediums_count);
}

INSTANTIATE_TEST_SUITE_P(ParameterizedBasePcpHandlerTest, BasePcpHandlerTest,
                         ::testing::ValuesIn(kTestCases));

}  // namespace
}  // namespace connections
}  // namespace nearby
}  // namespace location
