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

#include <memory>

#include "core_v2/internal/base_endpoint_channel.h"
#include "core_v2/internal/client_proxy.h"
#include "core_v2/internal/encryption_runner.h"
#include "core_v2/internal/offline_frames.h"
#include "core_v2/listeners.h"
#include "core_v2/params.h"
#include "proto/connections/offline_wire_formats.pb.h"
#include "platform_v2/base/byte_array.h"
#include "platform_v2/public/count_down_latch.h"
#include "platform_v2/public/pipe.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/time/time.h"

namespace location {
namespace nearby {
namespace connections {
namespace {

using ::location::nearby::proto::connections::Medium;
using ::testing::_;
using ::testing::Invoke;
using ::testing::MockFunction;
using ::testing::Return;
using ::testing::StrictMock;

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
  MockPcpHandler(EndpointManager* em, EndpointChannelManager* ecm)
      : BasePcpHandler(em, ecm) {}

  // Expose protected inner types of a base type for mocking.
  using BasePcpHandler::ConnectImplResult;
  using BasePcpHandler::DiscoveredEndpoint;
  using BasePcpHandler::StartOperationResult;

  MOCK_METHOD(Strategy, GetStrategy, (), (override));
  MOCK_METHOD(Pcp, GetPcp, (), (override));

  MOCK_METHOD(StartOperationResult, StartAdvertisingImpl,
              (ClientProxy * client, const string& service_id,
               const string& local_endpoint_id,
               const string& local_endpoint_name,
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
  MOCK_METHOD(std::vector<proto::connections::Medium>,
              GetConnectionMediumsByPriority, (), (override));
  MOCK_METHOD(proto::connections::Medium, GetDefaultUpgradeMedium, (),
              (override));

  // Mock adapters for protected non-virtual methods of a base class.
  void OnEndpointFound(ClientProxy* client,
                       std::unique_ptr<DiscoveredEndpoint> endpoint) {
    BasePcpHandler::OnEndpointFound(client, std::move(endpoint));
  }
  void OnEndpointLost(ClientProxy* client, DiscoveredEndpoint* endpoint) {
    BasePcpHandler::OnEndpointLost(client, endpoint);
  }
};

class MockDiscoveredEndpoint final : public MockPcpHandler::DiscoveredEndpoint {
 public:
  MOCK_METHOD(std::string, GetEndpointId, (), (const override));
  MOCK_METHOD(std::string, GetEndpointName, (), (const override));
  MOCK_METHOD(std::string, GetServiceId, (), (const override));
  MOCK_METHOD(Medium, GetMedium, (), (const override));
};

class BasePcpHandlerTest : public ::testing::Test {
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
                                 const std::string& endpoint_name,
                                 const std::string& service_id)>>
        endpoint_found_cb;
    StrictMock<MockFunction<void(const std::string& endpoint_id)>>
        endpoint_lost_cb;
    StrictMock<
        MockFunction<void(const std::string& endpoint_id, DistanceInfo info)>>
        endpoint_distance_changed_cb;
  };

  void StartAdvertising(ClientProxy* client, MockPcpHandler* pcp_handler) {
    std::string service_id{"service"};
    ConnectionOptions options{
        .strategy = Strategy::kP2pCluster,
        .auto_upgrade_bandwidth = true,
        .enforce_topology_constraints = true,
    };
    ConnectionRequestInfo info{
        .name = "remote_endpoint_name",
        .listener = connection_listener_,
    };
    EXPECT_CALL(*pcp_handler,
                StartAdvertisingImpl(client, service_id, _, info.name, _))
        .WillOnce(Return(MockPcpHandler::StartOperationResult{
            .status = {Status::kSuccess},
            .mediums = {Medium::BLE},
        }));
    EXPECT_EQ(pcp_handler->StartAdvertising(client, service_id, options, info),
              Status{Status::kSuccess});
    EXPECT_TRUE(client->IsAdvertising());
  }

  void StartDiscovery(ClientProxy* client, MockPcpHandler* pcp_handler) {
    std::string service_id{"service"};
    ConnectionOptions options{
        .strategy = Strategy::kP2pCluster,
        .auto_upgrade_bandwidth = true,
        .enforce_topology_constraints = true,
    };
    EXPECT_CALL(*pcp_handler, StartDiscoveryImpl(client, service_id, _))
        .WillOnce(Return(MockPcpHandler::StartOperationResult{
            .status = {Status::kSuccess},
            .mediums = {Medium::BLE},
        }));
    EXPECT_EQ(pcp_handler->StartDiscovery(client, service_id, options,
                                          discovery_listener_),
              Status{Status::kSuccess});
    EXPECT_TRUE(client->IsDiscovering());
  }

  std::pair<std::unique_ptr<MockEndpointChannel>,
            std::unique_ptr<MockEndpointChannel>>
  SetupConnection(Pipe& pipe_a, Pipe& pipe_b) {  // NOLINT
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
    EXPECT_CALL(*channel_a, GetMedium).WillRepeatedly(Return(Medium::BLE));
    EXPECT_CALL(*channel_a, GetLastReadTimestamp)
        .WillRepeatedly(Return(absl::Now()));
    EXPECT_CALL(*channel_a, IsPaused)
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*channel_b, Read())
        .WillRepeatedly(Invoke(
            [channel = channel_b.get()]() { return channel->DoRead(); }));
    EXPECT_CALL(*channel_b, Write(_))
        .WillRepeatedly(
            Invoke([channel = channel_b.get()](const ByteArray& data) {
              return channel->DoWrite(data);
            }));
    EXPECT_CALL(*channel_b, GetMedium).WillRepeatedly(Return(Medium::BLE));
    EXPECT_CALL(*channel_b, GetLastReadTimestamp)
        .WillRepeatedly(Return(absl::Now()));
    EXPECT_CALL(*channel_b, IsPaused)
        .WillRepeatedly(Return(false));
    return std::make_pair(std::move(channel_a), std::move(channel_b));
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

TEST_F(BasePcpHandlerTest, ConstructorDestructorWorks) {
  auto ecm = std::make_unique<EndpointChannelManager>();
  auto em = std::make_unique<EndpointManager>(ecm.get());
  auto pcp_handler = std::make_unique<MockPcpHandler>(em.get(), ecm.get());
  SUCCEED();
}

TEST_F(BasePcpHandlerTest, StartAdvertisingChangesState) {
  auto client = std::make_unique<ClientProxy>();
  auto ecm = std::make_unique<EndpointChannelManager>();
  auto em = std::make_unique<EndpointManager>(ecm.get());
  auto pcp_handler = std::make_unique<MockPcpHandler>(em.get(), ecm.get());
  StartAdvertising(client.get(), pcp_handler.get());
}

TEST_F(BasePcpHandlerTest, StopAdvertisingChangesState) {
  auto client = std::make_unique<ClientProxy>();
  auto ecm = std::make_unique<EndpointChannelManager>();
  auto em = std::make_unique<EndpointManager>(ecm.get());
  auto pcp_handler = std::make_unique<MockPcpHandler>(em.get(), ecm.get());
  StartAdvertising(client.get(), pcp_handler.get());
  EXPECT_CALL(*pcp_handler, StopAdvertisingImpl(client.get())).Times(1);
  EXPECT_TRUE(client->IsAdvertising());
  pcp_handler->StopAdvertising(client.get());
  EXPECT_FALSE(client->IsAdvertising());
}

TEST_F(BasePcpHandlerTest, StartDiscoveryChangesState) {
  auto client = std::make_unique<ClientProxy>();
  auto ecm = std::make_unique<EndpointChannelManager>();
  auto em = std::make_unique<EndpointManager>(ecm.get());
  auto pcp_handler = std::make_unique<MockPcpHandler>(em.get(), ecm.get());
  StartDiscovery(client.get(), pcp_handler.get());
}

TEST_F(BasePcpHandlerTest, StopDiscoveryChangesState) {
  auto client = std::make_unique<ClientProxy>();
  auto ecm = std::make_unique<EndpointChannelManager>();
  auto em = std::make_unique<EndpointManager>(ecm.get());
  auto pcp_handler = std::make_unique<MockPcpHandler>(em.get(), ecm.get());
  StartDiscovery(client.get(), pcp_handler.get());
  EXPECT_CALL(*pcp_handler, StopDiscoveryImpl(client.get())).Times(1);
  EXPECT_TRUE(client->IsDiscovering());
  pcp_handler->StopDiscovery(client.get());
  EXPECT_FALSE(client->IsDiscovering());
}

}  // namespace
}  // namespace connections
}  // namespace nearby
}  // namespace location
