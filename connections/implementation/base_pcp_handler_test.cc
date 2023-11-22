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

#include "connections/implementation/base_pcp_handler.h"

#include <array>
#include <atomic>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/base/thread_annotations.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "connections/advertising_options.h"
#include "connections/connection_options.h"
#include "connections/discovery_options.h"
#include "connections/implementation/analytics/packet_meta_data.h"
#include "connections/implementation/base_endpoint_channel.h"
#include "connections/implementation/bwu_manager.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/encryption_runner.h"
#include "connections/implementation/endpoint_manager.h"
#include "connections/implementation/flags/nearby_connections_feature_flags.h"
#include "connections/implementation/mediums/mediums.h"
#include "connections/implementation/mock_device.h"
#include "connections/implementation/offline_frames.h"
#include "connections/implementation/pcp.h"
#include "connections/implementation/proto/offline_wire_formats.pb.h"
#include "connections/listeners.h"
#include "connections/medium_selector.h"
#include "connections/out_of_band_connection_metadata.h"
#include "connections/params.h"
#include "connections/status.h"
#include "connections/strategy.h"
#include "connections/v3/connection_listening_options.h"
#include "internal/flags/nearby_flags.h"
#include "internal/interop/device.h"
#include "internal/interop/device_provider.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/feature_flags.h"
#include "internal/platform/logging.h"
#include "internal/platform/medium_environment.h"
#include "internal/platform/output_stream.h"
#include "internal/platform/pipe.h"
#include "proto/connections_enums.pb.h"

namespace nearby {
namespace connections {
namespace {

using ::location::nearby::connections::OsInfo;
using ::location::nearby::proto::connections::Medium;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::Invoke;
using ::testing::MockFunction;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrictMock;

constexpr absl::string_view kTestEndpointId = "REMOTETEST";

constexpr std::array<char, 6> kFakeMacAddress = {'a', 'b', 'c', 'd', 'e', 'f'};

constexpr BooleanMediumSelector kTestCases[] = {
    BooleanMediumSelector{},
    BooleanMediumSelector{
        .ble = true,
    },
    BooleanMediumSelector{
        .bluetooth = true,
    },
    BooleanMediumSelector{
        .wifi_lan = true,
    },
    BooleanMediumSelector{
        .bluetooth = true,
        .ble = true,
    },
    BooleanMediumSelector{
        .bluetooth = true,
        .wifi_lan = true,
    },
    BooleanMediumSelector{
        .ble = true,
        .wifi_lan = true,
    },
    BooleanMediumSelector{
        .bluetooth = true,
        .ble = true,
        .wifi_lan = true,
    },
};

class FakePresenceDevice : public NearbyDevice {
 public:
  std::string GetEndpointId() const override { return "LOCALTEST"; }
  MOCK_METHOD(std::vector<ConnectionInfoVariant>, GetConnectionInfos, (),
              (const override));
  MOCK_METHOD(NearbyDevice::Type, GetType, (), (const override));
  MOCK_METHOD(std::string, ToProtoBytes, (), (const override));
};

class FakePresenceDeviceProvider : public NearbyDeviceProvider {
 public:
  const NearbyDevice* GetLocalDevice() override { return &local_device_; }
  FakePresenceDevice local_device_;
};

class MockEndpointChannel : public BaseEndpointChannel {
 public:
  explicit MockEndpointChannel(std::unique_ptr<InputStream> reader,
                               std::unique_ptr<OutputStream> writer)
      : BaseEndpointChannel("service_id", "channel", reader.get(),
                            writer.get()),
        input_stream_(std::move(reader)),
        output_stream_(std::move(writer)) {}

  ExceptionOr<ByteArray> DoRead() { return BaseEndpointChannel::Read(); }
  Exception DoWrite(const ByteArray& data) {
    if (broken_write_) {
      return {Exception::kFailed};
    }
    return BaseEndpointChannel::Write(data);
  }
  absl::Time DoGetLastReadTimestamp() {
    return BaseEndpointChannel::GetLastReadTimestamp();
  }

  MOCK_METHOD(ExceptionOr<ByteArray>, Read, (), (override));
  MOCK_METHOD(Exception, Write, (const ByteArray& data), (override));
  MOCK_METHOD(void, CloseImpl, (), (override));
  MOCK_METHOD(location::nearby::proto::connections::Medium, GetMedium, (),
              (const override));
  MOCK_METHOD(std::string, GetType, (), (const override));
  MOCK_METHOD(std::string, GetName, (), (const override));
  MOCK_METHOD(bool, IsPaused, (), (const override));
  MOCK_METHOD(void, Pause, (), (override));
  MOCK_METHOD(void, Resume, (), (override));
  MOCK_METHOD(absl::Time, GetLastReadTimestamp, (), (const override));

  bool broken_write_{false};

 private:
  std::unique_ptr<InputStream> input_stream_;
  std::unique_ptr<OutputStream> output_stream_;
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
               const AdvertisingOptions& advertising_options),
              (override));
  MOCK_METHOD(Status, StopAdvertisingImpl, (ClientProxy * client), (override));
  MOCK_METHOD(StartOperationResult, StartDiscoveryImpl,
              (ClientProxy * client, const std::string& service_id,
               const DiscoveryOptions& discovery_options),
              (override));
  MOCK_METHOD(Status, StopDiscoveryImpl, (ClientProxy * client), (override));
  MOCK_METHOD(StartOperationResult, StartListeningForIncomingConnectionsImpl,
              (ClientProxy * client_proxy, absl::string_view service_id,
               absl::string_view local_endpoint_id,
               v3::ConnectionListeningOptions options),
              (override));
  MOCK_METHOD(void, StopListeningForIncomingConnectionsImpl,
              (ClientProxy * client_proxy), (override));
  MOCK_METHOD(Status, InjectEndpointImpl,
              (ClientProxy * client, const std::string& service_id,
               const OutOfBandConnectionMetadata& metadata),
              (override));
  MOCK_METHOD(ConnectImplResult, ConnectImpl,
              (ClientProxy * client, DiscoveredEndpoint* endpoint), (override));
  MOCK_METHOD(location::nearby::proto::connections::Medium,
              GetDefaultUpgradeMedium, (), (override));
  MOCK_METHOD(StartOperationResult, UpdateAdvertisingOptionsImpl,
              (ClientProxy*, absl::string_view, absl::string_view,
               absl::string_view, const AdvertisingOptions&),
              (override));
  MOCK_METHOD(StartOperationResult, UpdateDiscoveryOptionsImpl,
              (ClientProxy*, absl::string_view, absl::string_view,
               absl::string_view, const DiscoveryOptions&),
              (override));

  std::vector<location::nearby::proto::connections::Medium>
  GetConnectionMediumsByPriority() override {
    return std::vector<location::nearby::proto::connections::Medium>{
        location::nearby::proto::connections::WIFI_LAN,
        location::nearby::proto::connections::WEB_RTC,
        location::nearby::proto::connections::BLUETOOTH,
        location::nearby::proto::connections::BLE};
  }

  // Mock adapters for protected non-virtual methods of a base class.
  void OnEndpointFound(ClientProxy* client,
                       std::shared_ptr<DiscoveredEndpoint> endpoint)
      ABSL_NO_THREAD_SAFETY_ANALYSIS {
    BasePcpHandler::OnEndpointFound(client, std::move(endpoint));
  }
  void OnEndpointLost(ClientProxy* client, const DiscoveredEndpoint& endpoint)
      ABSL_NO_THREAD_SAFETY_ANALYSIS {
    BasePcpHandler::OnEndpointLost(client, endpoint);
  }
  BasePcpHandler::DiscoveredEndpoint* GetDiscoveredEndpoint(
      const std::string& endpoint_id) {
    return BasePcpHandler::GetDiscoveredEndpoint(endpoint_id);
  }
  std::vector<BasePcpHandler::DiscoveredEndpoint*> GetDiscoveredEndpoints(
      const std::string& endpoint_id) {
    return BasePcpHandler::GetDiscoveredEndpoints(endpoint_id);
  }

  std::vector<BasePcpHandler::DiscoveredEndpoint*> GetDiscoveredEndpoints(
      location::nearby::proto::connections::Medium medium) {
    return BasePcpHandler::GetDiscoveredEndpoints(medium);
  }

  int GetEndpointLostByMediumAlarmsCount() {
    Future<int> alarms_count;
    RunOnPcpHandlerThread(
        "GetEndpointLostByMediumAlarmsCount",
        [this, alarms_count]() RUN_ON_PCP_HANDLER_THREAD() mutable {
          alarms_count.Set(
              BasePcpHandler::GetEndpointLostByMediumAlarmsCount());
        });
    return alarms_count.Get().result();
  }

  void StartEndpointLostByMediumAlarms(
      ClientProxy* client,
      location::nearby::proto::connections::Medium medium) {
    RunOnPcpHandlerThread("StartEndpointLostByMediumAlarms",
                          [this, client, medium]() RUN_ON_PCP_HANDLER_THREAD() {
                            BasePcpHandler::StartEndpointLostByMediumAlarms(
                                client, medium);
                          });
  }

  void StopEndpointLostByMediumAlarm(
      absl::string_view endpoint_id,
      location::nearby::proto::connections::Medium medium) {
    RunOnPcpHandlerThread("StopEndpointLostByMediumAlarm",
                          [this, endpoint_id = std::string(endpoint_id),
                           medium]() RUN_ON_PCP_HANDLER_THREAD() {
                            BasePcpHandler::StopEndpointLostByMediumAlarm(
                                endpoint_id, medium);
                          });
  }

  std::vector<location::nearby::proto::connections::Medium> GetDiscoveryMediums(
      ClientProxy* client) {
    auto allowed = client->GetDiscoveryOptions().CompatibleOptions().allowed;
    return GetMediumsFromSelector(allowed);
  }

  std::vector<location::nearby::proto::connections::Medium>
  GetMediumsFromSelector(BooleanMediumSelector allowed) {
    return allowed.GetMediums(true);
  }

  std::vector<ConnectionInfoVariant> GetConnectionInfoFromResult(
      absl::string_view service_id,
      BasePcpHandler::StartOperationResult result) {
    return BasePcpHandler::GetConnectionInfoFromResult(service_id, result);
  }

  Exception OnIncomingConnection(
      ClientProxy* client, const ByteArray& remote_endpoint_info,
      std::unique_ptr<EndpointChannel> endpoint_channel,
      location::nearby::proto::connections::Medium medium,
      NearbyDevice::Type listening_device_type) {
    return BasePcpHandler::OnIncomingConnection(client, remote_endpoint_info,
                                                std::move(endpoint_channel),
                                                medium, listening_device_type);
  }

  bool NeedsToTurnOffAdvertisingMedium(
      location::nearby::proto::connections::Medium medium,
      const AdvertisingOptions& old_options,
      const AdvertisingOptions& new_options) {
    return BasePcpHandler::NeedsToTurnOffAdvertisingMedium(medium, old_options,
                                                           new_options);
  }

  bool NeedsToTurnOffDiscoveryMedium(
      location::nearby::proto::connections::Medium medium,
      const DiscoveryOptions& old_options,
      const DiscoveryOptions& new_options) {
    return BasePcpHandler::NeedsToTurnOffDiscoveryMedium(medium, old_options,
                                                         new_options);
  }
};

class MockContext {
 public:
  explicit MockContext(std::atomic_int* destroyed = nullptr)
      : destroyed_{destroyed} {}
  MockContext(MockContext&& other) { *this = std::move(other); }
  MockContext& operator=(MockContext&& other) {
    destroyed_ = other.destroyed_;
    other.destroyed_ = nullptr;
    return *this;
  }

  ~MockContext() {
    if (destroyed_) (*destroyed_)++;
  }

 private:
  std::atomic_int* destroyed_;
};

struct MockDiscoveredEndpoint : public MockPcpHandler::DiscoveredEndpoint {
  MockDiscoveredEndpoint(DiscoveredEndpoint endpoint, MockContext context)
      : DiscoveredEndpoint(std::move(endpoint)), context(std::move(context)) {}

  MockContext context;
};

class SetSafeToDisconnect {
 public:
  explicit SetSafeToDisconnect(bool safe_to_disconnect) {
    NearbyFlags::GetInstance().OverrideBoolFlagValue(
        config_package_nearby::nearby_connections_feature::
            kEnableSafeToDisconnect,
        safe_to_disconnect);
  }
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
    StrictMock<
        MockFunction<void(const std::string& endpoint_id, Medium medium)>>
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
    AdvertisingOptions advertising_options{
        {
            Strategy::kP2pCluster,
            allowed,
        },
        true,  // auto_upgrade_bandwidth
        true,  // enforce_topology_constraints
    };
    StartAdvertisingWithOptions(client, pcp_handler, advertising_options);
  }

  void StartAdvertisingWithOptions(ClientProxy* client,
                                   MockPcpHandler* pcp_handler,
                                   AdvertisingOptions advertising_options) {
    std::string service_id{"service"};
    ConnectionRequestInfo info{
        .endpoint_info = ByteArray{"remote_endpoint_name"},
        .listener = connection_listener_,
    };
    EXPECT_CALL(*pcp_handler, StartAdvertisingImpl(client, service_id, _,
                                                   info.endpoint_info, _))
        .WillOnce(Return(MockPcpHandler::StartOperationResult{
            .status = {Status::kSuccess},
            .mediums = pcp_handler->GetMediumsFromSelector(
                advertising_options.allowed),
        }));
    EXPECT_EQ(pcp_handler->StartAdvertising(client, service_id,
                                            advertising_options, info),
              Status{Status::kSuccess});
    EXPECT_TRUE(client->IsAdvertising());
    EXPECT_EQ(client->GetLocalEndpointInfo(), info.endpoint_info.string_data());
  }

  void UpdateAdvertisingOptions(ClientProxy* client,
                                MockPcpHandler* pcp_handler,
                                AdvertisingOptions new_options,
                                Status expected_status) {
    EXPECT_CALL(*pcp_handler, UpdateAdvertisingOptionsImpl)
        .WillOnce(Return(MockPcpHandler::StartOperationResult{
            .status = expected_status,
            .mediums = pcp_handler->GetMediumsFromSelector(new_options.allowed),
        }));
    pcp_handler->UpdateAdvertisingOptions(client, "service", new_options);
  }

  void StartDiscovery(ClientProxy* client, MockPcpHandler* pcp_handler,
                      BooleanMediumSelector allowed = GetParam()) {
    DiscoveryOptions discovery_options{
        {
            Strategy::kP2pCluster,
            allowed,
        },
        true,  // auto_upgrade_bandwidth
        true,  // enforce_topology_constraints
    };
    StartDiscoveryWithOptions(client, pcp_handler, discovery_options);
  }

  void StartDiscoveryWithOptions(ClientProxy* client,
                                 MockPcpHandler* pcp_handler,
                                 const DiscoveryOptions& discovery_options) {
    std::string service_id("service");
    EXPECT_CALL(*pcp_handler, StartDiscoveryImpl(client, service_id, _))
        .WillOnce(Return(MockPcpHandler::StartOperationResult{
            .status = {Status::kSuccess},
            .mediums =
                pcp_handler->GetMediumsFromSelector(discovery_options.allowed),
        }));
    EXPECT_EQ(pcp_handler->StartDiscovery(client, service_id, discovery_options,
                                          GetDiscoveryListener()),
              Status{Status::kSuccess});
    EXPECT_TRUE(client->IsDiscovering());
    for (const auto& discovered_medium :
         pcp_handler->GetDiscoveryMediums(client)) {
      EXPECT_TRUE(
          pcp_handler->GetDiscoveredEndpoints(discovered_medium).empty());
    }
  }

  void UpdateDiscoveryOptions(ClientProxy* client, MockPcpHandler* pcp_handler,
                              DiscoveryOptions new_options,
                              Status expected_status) {
    EXPECT_CALL(*pcp_handler, UpdateDiscoveryOptionsImpl)
        .WillOnce(Return(MockPcpHandler::StartOperationResult{
            .status = expected_status,
            .mediums = pcp_handler->GetMediumsFromSelector(new_options.allowed),
        }));
    pcp_handler->UpdateDiscoveryOptions(client, "service", new_options);
  }

  std::pair<std::unique_ptr<MockEndpointChannel>,
            std::unique_ptr<MockEndpointChannel>>
  SetupConnection(
      location::nearby::proto::connections::Medium medium) {  // NOLINT
    auto [input_a, output_a] = CreatePipe();
    auto [input_b, output_b] = CreatePipe();
    auto channel_a = std::make_unique<MockEndpointChannel>(std::move(input_a),
                                                           std::move(output_b));
    auto channel_b = std::make_unique<MockEndpointChannel>(std::move(input_b),
                                                           std::move(output_a));
    ON_CALL(mock_device_, GetType)
        .WillByDefault(Return(NearbyDevice::Type::kUnknownDevice));
    ON_CALL(mock_device_, GetEndpointId)
        .WillByDefault(Return(std::string(kTestEndpointId)));
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

  std::pair<std::unique_ptr<MockEndpointChannel>,
            std::unique_ptr<MockEndpointChannel>>
  SetupConnectionForConnectFailure(
      location::nearby::proto::connections::Medium medium) {  // NOLINT
    auto [input_a, output_a] = CreatePipe();
    auto [input_b, output_b] = CreatePipe();
    auto channel_a = std::make_unique<MockEndpointChannel>(std::move(input_a),
                                                           std::move(output_b));
    auto channel_b = std::make_unique<MockEndpointChannel>(std::move(input_b),
                                                           std::move(output_a));
    ON_CALL(mock_device_, GetType)
        .WillByDefault(Return(NearbyDevice::Type::kUnknownDevice));
    ON_CALL(mock_device_, GetEndpointId)
        .WillByDefault(Return(std::string(kTestEndpointId)));
    // On initiator (A) side, we drop the first write, since this is a
    // connection establishment packet, and we don't have the peer entity, just
    // the peer channel. The rest of the exchange must happen for the benefit of
    // DH key exchange.
    EXPECT_CALL(*channel_a, Read())
        .WillRepeatedly(Invoke(
            [channel = channel_a.get()]() { return channel->DoRead(); }));
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

  void RequestConnection(
      const std::string& endpoint_id,
      std::unique_ptr<MockEndpointChannel> channel_a,
      MockEndpointChannel* channel_b, ClientProxy* client,
      MockPcpHandler* pcp_handler,
      location::nearby::proto::connections::Medium connect_medium,
      std::atomic_int* flag = nullptr,
      Status expected_result = {Status::kSuccess}) {
    ConnectionRequestInfo info{
        .endpoint_info = ByteArray{"ABCD"},
        .listener = connection_listener_,
    };
    ConnectionOptions connection_options{
        .remote_bluetooth_mac_address =
            ByteArray{std::string("\x12\x34\x56\x78\x9a\xbc")},
        .keep_alive_interval_millis =
            FeatureFlags::GetInstance().GetFlags().keep_alive_interval_millis,
        .keep_alive_timeout_millis =
            FeatureFlags::GetInstance().GetFlags().keep_alive_timeout_millis,
    };
    EXPECT_CALL(mock_discovery_listener_.endpoint_found_cb, Call);
    EXPECT_CALL(*pcp_handler, CanSendOutgoingConnection)
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*pcp_handler, GetStrategy)
        .WillRepeatedly(Return(Strategy::kP2pCluster));
    if (expected_result == Status{Status::kSuccess}) {
      EXPECT_CALL(mock_connection_listener_.initiated_cb, Call).Times(1);
    }
    // Simulate successful discovery.
    auto encryption_runner = std::make_unique<EncryptionRunner>();
    auto allowed_mediums = pcp_handler->GetDiscoveryMediums(client);

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
                  WebRtcState::kUndefined,
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
    EXPECT_EQ(pcp_handler->RequestConnection(client, endpoint_id, info,
                                             connection_options),
              expected_result);
    NEARBY_LOG(INFO, "Stopping Encryption Runner");
  }

  void RequestConnectionV3(
      const NearbyDevice& remote_device,
      std::unique_ptr<MockEndpointChannel> channel_a,
      MockEndpointChannel* channel_b, ClientProxy* client,
      MockPcpHandler* pcp_handler,
      location::nearby::proto::connections::Medium connect_medium,
      std::atomic_int* flag = nullptr,
      Status expected_result = {Status::kSuccess}) {
    ConnectionRequestInfo info{
        .endpoint_info = ByteArray{"ABCD"},
        .listener = connection_listener_,
    };
    ConnectionOptions connection_options{
        .remote_bluetooth_mac_address = ByteArray{"\x12\x34\x56\x78\x9a\xbc"},
        .keep_alive_interval_millis =
            FeatureFlags::GetInstance().GetFlags().keep_alive_interval_millis,
        .keep_alive_timeout_millis =
            FeatureFlags::GetInstance().GetFlags().keep_alive_timeout_millis,
    };
    EXPECT_CALL(mock_discovery_listener_.endpoint_found_cb, Call);
    EXPECT_CALL(*pcp_handler, CanSendOutgoingConnection)
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*pcp_handler, GetStrategy)
        .WillRepeatedly(Return(Strategy::kP2pCluster));
    if (expected_result == Status{Status::kSuccess}) {
      EXPECT_CALL(mock_connection_listener_.initiated_cb, Call).Times(1);
    }
    // Simulate successful discovery.
    auto encryption_runner = std::make_unique<EncryptionRunner>();
    auto allowed_mediums = pcp_handler->GetDiscoveryMediums(client);

    EXPECT_CALL(*pcp_handler, ConnectImpl)
        .WillRepeatedly(
            Invoke([&channel_a, connect_medium](
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
                  remote_device.GetEndpointId(),
                  info.endpoint_info,
                  "service",
                  discovered_medium,
                  WebRtcState::kUndefined,
              },
              MockContext{flag},
          }));
    }
    auto other_client = std::make_unique<ClientProxy>();

    // Run peer crypto in advance, if channel_b is provided.
    // Otherwise stay in not-encrypted state.
    if (channel_b != nullptr) {
      encryption_runner->StartServer(
          other_client.get(), remote_device.GetEndpointId(), channel_b, {});
    }
    EXPECT_EQ(pcp_handler->RequestConnectionV3(client, remote_device, info,
                                               connection_options),
              expected_result);
  }

  void RequestConnectionWifiLanFail(
      const std::string& endpoint_id,
      std::unique_ptr<MockEndpointChannel> channel_a,
      MockEndpointChannel* channel_b, ClientProxy* client,
      MockPcpHandler* pcp_handler, std::atomic_int* flag = nullptr,
      Status expected_result = {Status::kSuccess}) {
    ConnectionRequestInfo info{
        .endpoint_info = ByteArray{"ABCD"},
        .listener = connection_listener_,
    };
    ConnectionOptions connection_options{
        .remote_bluetooth_mac_address =
            ByteArray{std::string("\x12\x34\x56\x78\x9a\xbc")},
        .keep_alive_interval_millis =
            FeatureFlags::GetInstance().GetFlags().keep_alive_interval_millis,
        .keep_alive_timeout_millis =
            FeatureFlags::GetInstance().GetFlags().keep_alive_timeout_millis,
    };
    EXPECT_CALL(mock_discovery_listener_.endpoint_found_cb, Call);
    EXPECT_CALL(*pcp_handler, CanSendOutgoingConnection)
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*pcp_handler, GetStrategy)
        .WillRepeatedly(Return(Strategy::kP2pCluster));
    if (expected_result == Status{Status::kSuccess}) {
      EXPECT_CALL(mock_connection_listener_.initiated_cb, Call).Times(1);
    }
    // Simulate successful discovery.
    auto encryption_runner = std::make_unique<EncryptionRunner>();
    auto allowed_mediums = pcp_handler->GetDiscoveryMediums(client);

    EXPECT_CALL(*pcp_handler, ConnectImpl)
        .WillRepeatedly(
            Invoke([&channel_a](ClientProxy* client,
                                MockPcpHandler::DiscoveredEndpoint* endpoint) {
              if (endpoint->medium ==
                  location::nearby::proto::connections::WIFI_LAN) {
                NEARBY_LOGS(INFO) << "Connect with Medium WIFI_LAN failed.";
                return MockPcpHandler::ConnectImplResult{
                    .medium = endpoint->medium,
                    .status = {Status::kError},
                    .endpoint_channel = nullptr,
                };
              } else {
                NEARBY_LOGS(INFO)
                    << "Connect with Medium: "
                    << location::nearby::proto::connections::Medium_Name(
                           endpoint->medium);
                return MockPcpHandler::ConnectImplResult{
                    .medium = endpoint->medium,
                    .status = {Status::kSuccess},
                    .endpoint_channel = std::move(channel_a),
                };
              }
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
                  WebRtcState::kUndefined,
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
    EXPECT_EQ(pcp_handler->RequestConnection(client, endpoint_id, info,
                                             connection_options),
              expected_result);
    NEARBY_LOG(INFO, "Stopping Encryption Runner");
  }
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
  DiscoveryListener GetDiscoveryListener() {
    return DiscoveryListener{
        .endpoint_found_cb =
            mock_discovery_listener_.endpoint_found_cb.AsStdFunction(),
        .endpoint_lost_cb =
            mock_discovery_listener_.endpoint_lost_cb.AsStdFunction(),
        .endpoint_distance_changed_cb =
            mock_discovery_listener_.endpoint_distance_changed_cb
                .AsStdFunction(),
    };
  }
  SetSafeToDisconnect set_safe_to_disconnect_{true};
  MediumEnvironment& env_ = MediumEnvironment::Instance();
  NiceMock<MockNearbyDevice> mock_device_;
};

TEST_P(BasePcpHandlerTest, ConstructorDestructorWorks) {
  env_.Start();
  Mediums m;
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  BwuManager bwu(m, em, ecm, {}, {});
  MockPcpHandler pcp_handler(&m, &em, &ecm, &bwu);
  SUCCEED();
  bwu.Shutdown();
  env_.Stop();
}

TEST_P(BasePcpHandlerTest, StartAdvertisingChangesState) {
  env_.Start();
  ClientProxy client;
  Mediums m;
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  BwuManager bwu(m, em, ecm, {}, {});
  MockPcpHandler pcp_handler(&m, &em, &ecm, &bwu);
  StartAdvertising(&client, &pcp_handler);
  bwu.Shutdown();
  env_.Stop();
}

TEST_P(BasePcpHandlerTest, StopAdvertisingChangesState) {
  env_.Start();
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
  bwu.Shutdown();
  env_.Stop();
}

TEST_P(BasePcpHandlerTest, StartDiscoveryChangesState) {
  env_.Start();
  ClientProxy client;
  Mediums m;
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  BwuManager bwu(m, em, ecm, {}, {});
  MockPcpHandler pcp_handler(&m, &em, &ecm, &bwu);
  StartDiscovery(&client, &pcp_handler);
  bwu.Shutdown();
  env_.Stop();
}

TEST_P(BasePcpHandlerTest, StartDiscoveryFails) {
  env_.Start();
  ClientProxy client;
  Mediums m;
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  BwuManager bwu(m, em, ecm, {}, {});
  MockPcpHandler pcp_handler(&m, &em, &ecm, &bwu);
  DiscoveryOptions discovery_options{
      {},
      true,   // auto_upgrade_bandwidth
      true,   // enforce_topology_constraints
      false,  // is_out_of_band_connection,
      "",     // fast_advertisement_service_uuid
      true,   // low_power
  };
  EXPECT_CALL(pcp_handler, StartDiscoveryImpl)
      .WillOnce(Return(MockPcpHandler::StartOperationResult{
          .status = {Status::kError},
          .mediums = {},
      }));
  EXPECT_EQ(pcp_handler.StartDiscovery(&client, "service", discovery_options,
                                       GetDiscoveryListener()),
            Status{Status::kError});
  bwu.Shutdown();
  env_.Stop();
}

TEST_P(BasePcpHandlerTest, StopDiscoveryChangesState) {
  env_.Start();
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
  bwu.Shutdown();
  env_.Stop();
}

TEST_P(BasePcpHandlerTest, StartStopStartDiscoveryClearsEndpoints) {
  env_.Start();
  ClientProxy client;
  Mediums m;
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  BwuManager bwu(m, em, ecm, {}, {});
  MockPcpHandler pcp_handler(&m, &em, &ecm, &bwu);
  StartDiscovery(&client, &pcp_handler);
  auto mediums = pcp_handler.GetDiscoveryMediums(&client);
  auto connect_medium = mediums[mediums.size() - 1];
  EXPECT_CALL(mock_discovery_listener_.endpoint_found_cb, Call);
  pcp_handler.OnEndpointFound(
      &client, std::make_shared<MockDiscoveredEndpoint>(MockDiscoveredEndpoint{
                   {
                       "DEFG",
                       ByteArray("1"),
                       "service",
                       connect_medium,
                       WebRtcState::kUndefined,
                   },
                   MockContext{nullptr},
               }));
  EXPECT_CALL(pcp_handler, StopDiscoveryImpl(&client)).Times(1);
  pcp_handler.StopDiscovery(&client);
  EXPECT_FALSE(client.IsDiscovering());
  StartDiscovery(&client, &pcp_handler);
  bwu.Shutdown();
  env_.Stop();
}

TEST_F(BasePcpHandlerTest, WifiMediumFailFallBackToBT) {
  env_.Start();
  std::string service_id{"service"};
  std::string endpoint_id{"ABCD"};
  ClientProxy client;
  Mediums m;
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  BwuManager bwu(m, em, ecm, {}, {});
  MockPcpHandler pcp_handler(&m, &em, &ecm, &bwu);
  BooleanMediumSelector allowed{
      .bluetooth = true,
      .wifi_lan = true,
  };
  DiscoveryOptions discovery_options{
      {
          Strategy::kP2pCluster,
          allowed,
      },
      false,  // auto_upgrade_bandwidth;
      false,  // enforce_topology_constraints;
  };
  EXPECT_CALL(pcp_handler, StartDiscoveryImpl(&client, service_id, _))
      .WillOnce(Return(MockPcpHandler::StartOperationResult{
          .status = {Status::kSuccess},
          .mediums = allowed.GetMediums(true),
      }));

  EXPECT_EQ(pcp_handler.StartDiscovery(&client, service_id, discovery_options,
                                       GetDiscoveryListener()),
            Status{Status::kSuccess});
  EXPECT_TRUE(client.IsDiscovering());

  auto mediums = pcp_handler.GetDiscoveryMediums(&client);
  auto connect_medium = mediums[mediums.size() - 1];
  auto channel_pair = SetupConnection(connect_medium);
  auto& channel_a = channel_pair.first;
  auto& channel_b = channel_pair.second;
  EXPECT_CALL(*channel_a, CloseImpl).Times(1);
  EXPECT_CALL(*channel_b, CloseImpl).Times(1);
  EXPECT_CALL(mock_connection_listener_.rejected_cb, Call).Times(AtLeast(0));
  RequestConnectionWifiLanFail(endpoint_id, std::move(channel_a),
                               channel_b.get(), &client, &pcp_handler);
  NEARBY_LOG(INFO, "RequestConnection complete");
  channel_b->Close();
  bwu.Shutdown();
  pcp_handler.DisconnectFromEndpointManager();
  env_.Stop();
}

TEST_P(BasePcpHandlerTest, RequestConnectionChangesState) {
  env_.Start();
  ClientProxy client;
  Mediums m;
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  BwuManager bwu(m, em, ecm, {}, {});
  MockPcpHandler pcp_handler(&m, &em, &ecm, &bwu);
  StartDiscovery(&client, &pcp_handler);
  auto mediums = pcp_handler.GetDiscoveryMediums(&client);
  auto connect_medium = mediums[mediums.size() - 1];
  auto channel_pair = SetupConnection(connect_medium);
  auto& channel_a = channel_pair.first;
  auto& channel_b = channel_pair.second;
  EXPECT_CALL(*channel_a, CloseImpl).Times(1);
  EXPECT_CALL(*channel_b, CloseImpl).Times(1);
  EXPECT_CALL(mock_connection_listener_.rejected_cb, Call).Times(AtLeast(0));
  RequestConnection("1234", std::move(channel_a), channel_b.get(), &client,
                    &pcp_handler, connect_medium);
  NEARBY_LOG(INFO, "RequestConnection complete");
  channel_b->Close();
  bwu.Shutdown();
  pcp_handler.DisconnectFromEndpointManager();
  env_.Stop();
}

TEST_P(BasePcpHandlerTest, CanRequestConnectionPresence) {
  env_.Start();
  ClientProxy client;
  FakePresenceDeviceProvider provider;
  EXPECT_CALL(provider.local_device_, GetType)
      .WillRepeatedly(Return(NearbyDevice::Type::kPresenceDevice));
  EXPECT_CALL(provider.local_device_, ToProtoBytes).WillRepeatedly([]() {
    location::nearby::connections::PresenceDevice presence_device;
    presence_device.set_endpoint_id("TEST");
    presence_device.set_device_id(2468);
    presence_device.add_identity_type(1);
    presence_device.add_actions(1);
    presence_device.add_actions(2);
    presence_device.add_actions(3);
    presence_device.add_actions(4);
    presence_device.add_discovery_medium(
        location::nearby::connections::ConnectionRequestFrame::BLUETOOTH);
    std::string serialized = presence_device.SerializeAsString();
    EXPECT_FALSE(serialized.empty());
    return serialized;
  });
  client.RegisterDeviceProvider(&provider);
  Mediums m;
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  BwuManager bwu(m, em, ecm, {}, {});
  MockPcpHandler pcp_handler(&m, &em, &ecm, &bwu);
  StartDiscovery(&client, &pcp_handler);
  auto mediums = pcp_handler.GetDiscoveryMediums(&client);
  auto connect_medium = mediums[mediums.size() - 1];
  auto channel_pair = SetupConnection(connect_medium);
  auto& channel_a = channel_pair.first;
  auto& channel_b = channel_pair.second;
  EXPECT_CALL(*channel_a, CloseImpl).Times(1);
  EXPECT_CALL(*channel_b, CloseImpl).Times(1);
  EXPECT_CALL(mock_connection_listener_.rejected_cb, Call).Times(AtLeast(0));
  RequestConnection("1234", std::move(channel_a), channel_b.get(), &client,
                    &pcp_handler, connect_medium);
  NEARBY_LOG(INFO, "RequestConnection complete");
  channel_b->Close();
  bwu.Shutdown();
  pcp_handler.DisconnectFromEndpointManager();
}

TEST_P(BasePcpHandlerTest, CanRequestConnectionLegacy) {
  env_.Start();
  ClientProxy client;
  FakePresenceDeviceProvider provider;
  EXPECT_CALL(provider.local_device_, GetType)
      .WillRepeatedly(Return(NearbyDevice::Type::kUnknownDevice));
  EXPECT_CALL(provider.local_device_, ToProtoBytes);
  client.RegisterDeviceProvider(&provider);
  Mediums m;
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  BwuManager bwu(m, em, ecm, {}, {});
  MockPcpHandler pcp_handler(&m, &em, &ecm, &bwu);
  StartDiscovery(&client, &pcp_handler);
  auto mediums = pcp_handler.GetDiscoveryMediums(&client);
  auto connect_medium = mediums[mediums.size() - 1];
  auto channel_pair = SetupConnection(connect_medium);
  auto& channel_a = channel_pair.first;
  auto& channel_b = channel_pair.second;
  EXPECT_CALL(*channel_a, CloseImpl).Times(1);
  EXPECT_CALL(*channel_b, CloseImpl).Times(1);
  EXPECT_CALL(mock_connection_listener_.rejected_cb, Call).Times(AtLeast(0));
  RequestConnection("1234", std::move(channel_a), channel_b.get(), &client,
                    &pcp_handler, connect_medium);
  NEARBY_LOG(INFO, "RequestConnection complete");
  channel_b->Close();
  bwu.Shutdown();
  pcp_handler.DisconnectFromEndpointManager();
  env_.Stop();
}

TEST_P(BasePcpHandlerTest, RequestConnectionV3) {
  env_.Start();
  ClientProxy client;
  FakePresenceDeviceProvider provider;
  EXPECT_CALL(provider.local_device_, GetType)
      .WillRepeatedly(Return(NearbyDevice::Type::kUnknownDevice));
  EXPECT_CALL(provider.local_device_, ToProtoBytes);
  client.RegisterDeviceProvider(&provider);
  Mediums m;
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  BwuManager bwu(m, em, ecm, {}, {});
  MockPcpHandler pcp_handler(&m, &em, &ecm, &bwu);
  StartDiscovery(&client, &pcp_handler);
  auto mediums = pcp_handler.GetDiscoveryMediums(&client);
  auto connect_medium = mediums[mediums.size() - 1];
  auto channel_pair = SetupConnection(connect_medium);
  auto& channel_a = channel_pair.first;
  const auto& channel_b = channel_pair.second;
  EXPECT_CALL(*channel_a, CloseImpl).Times(1);
  EXPECT_CALL(*channel_b, CloseImpl).Times(1);
  EXPECT_CALL(mock_connection_listener_.rejected_cb, Call).Times(AtLeast(0));
  RequestConnectionV3(mock_device_, std::move(channel_a), channel_b.get(),
                      &client, &pcp_handler, connect_medium);
  NEARBY_LOG(INFO, "RequestConnectionV3 complete");
  channel_b->Close();
  bwu.Shutdown();
  pcp_handler.DisconnectFromEndpointManager();
  env_.Stop();
}

TEST_P(BasePcpHandlerTest, RequestConnectionV3_ConnectImplFailure) {
  env_.Start();
  ClientProxy client;
  FakePresenceDeviceProvider provider;
  EXPECT_CALL(provider.local_device_, GetType)
      .WillRepeatedly(Return(NearbyDevice::Type::kUnknownDevice));
  client.RegisterDeviceProvider(&provider);
  Mediums m;
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  BwuManager bwu(m, em, ecm, {}, {});
  MockPcpHandler pcp_handler(&m, &em, &ecm, &bwu);
  StartDiscovery(&client, &pcp_handler);
  auto mediums = pcp_handler.GetDiscoveryMediums(&client);
  auto connect_medium = mediums[mediums.size() - 1];
  auto channel_pair = SetupConnectionForConnectFailure(connect_medium);
  const auto& channel_b = channel_pair.second;
  EXPECT_CALL(*channel_b, CloseImpl).Times(1);
  EXPECT_CALL(mock_connection_listener_.rejected_cb, Call).Times(AtLeast(0));
  ConnectionRequestInfo info{
      .endpoint_info = ByteArray{"ABCD"},
      .listener = connection_listener_,
  };
  ConnectionOptions connection_options{
      .remote_bluetooth_mac_address = ByteArray{"\x12\x34\x56\x78\x9a\xbc"},
      .keep_alive_interval_millis =
          FeatureFlags::GetInstance().GetFlags().keep_alive_interval_millis,
      .keep_alive_timeout_millis =
          FeatureFlags::GetInstance().GetFlags().keep_alive_timeout_millis,
  };
  EXPECT_CALL(mock_discovery_listener_.endpoint_found_cb, Call);
  EXPECT_CALL(pcp_handler, CanSendOutgoingConnection)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(pcp_handler, GetStrategy)
      .WillRepeatedly(Return(Strategy::kP2pCluster));
  // Simulate successful discovery.
  auto encryption_runner = std::make_unique<EncryptionRunner>();
  auto allowed_mediums = pcp_handler.GetDiscoveryMediums(&client);

  EXPECT_CALL(pcp_handler, ConnectImpl)
      .WillRepeatedly(Invoke(
          [connect_medium](ClientProxy* client,
                           MockPcpHandler::DiscoveredEndpoint* endpoint) {
            return MockPcpHandler::ConnectImplResult{
                .medium = connect_medium,
                .status = {Status::kError},
                .endpoint_channel = nullptr,
            };
          }));

  for (const auto& discovered_medium : allowed_mediums) {
    pcp_handler.OnEndpointFound(
        &client,
        std::make_shared<MockDiscoveredEndpoint>(MockDiscoveredEndpoint{
            {
                mock_device_.GetEndpointId(),
                info.endpoint_info,
                "service",
                discovered_medium,
                WebRtcState::kUndefined,
            },
            MockContext{nullptr},
        }));
  }

  Status expected_result = {Status::kError};
  EXPECT_EQ(pcp_handler.RequestConnectionV3(&client, mock_device_, info,
                                            connection_options),
            expected_result);
  NEARBY_LOG(INFO, "RequestConnectionV3 complete");
  channel_b->Close();
  bwu.Shutdown();
  pcp_handler.DisconnectFromEndpointManager();
  env_.Stop();
}

TEST_P(BasePcpHandlerTest, RequestConnection_ConnectImplFailure) {
  env_.Start();
  ClientProxy client;
  FakePresenceDeviceProvider provider;
  EXPECT_CALL(provider.local_device_, GetType)
      .WillRepeatedly(Return(NearbyDevice::Type::kUnknownDevice));
  client.RegisterDeviceProvider(&provider);
  Mediums m;
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  BwuManager bwu(m, em, ecm, {}, {});
  MockPcpHandler pcp_handler(&m, &em, &ecm, &bwu);
  StartDiscovery(&client, &pcp_handler);
  auto mediums = pcp_handler.GetDiscoveryMediums(&client);
  auto connect_medium = mediums[mediums.size() - 1];
  auto channel_pair = SetupConnectionForConnectFailure(connect_medium);
  const auto& channel_b = channel_pair.second;
  EXPECT_CALL(*channel_b, CloseImpl).Times(1);
  EXPECT_CALL(mock_connection_listener_.rejected_cb, Call).Times(AtLeast(0));
  ConnectionRequestInfo info{
      .endpoint_info = ByteArray{"ABCD"},
      .listener = connection_listener_,
  };
  ConnectionOptions connection_options{
      .remote_bluetooth_mac_address = ByteArray{"\x12\x34\x56\x78\x9a\xbc"},
      .keep_alive_interval_millis =
          FeatureFlags::GetInstance().GetFlags().keep_alive_interval_millis,
      .keep_alive_timeout_millis =
          FeatureFlags::GetInstance().GetFlags().keep_alive_timeout_millis,
  };
  EXPECT_CALL(mock_discovery_listener_.endpoint_found_cb, Call);
  EXPECT_CALL(pcp_handler, CanSendOutgoingConnection)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(pcp_handler, GetStrategy)
      .WillRepeatedly(Return(Strategy::kP2pCluster));
  // Simulate successful discovery.
  auto encryption_runner = std::make_unique<EncryptionRunner>();
  auto allowed_mediums = pcp_handler.GetDiscoveryMediums(&client);

  EXPECT_CALL(pcp_handler, ConnectImpl)
      .WillRepeatedly(Invoke(
          [connect_medium](ClientProxy* client,
                           MockPcpHandler::DiscoveredEndpoint* endpoint) {
            return MockPcpHandler::ConnectImplResult{
                .medium = connect_medium,
                .status = {Status::kError},
                .endpoint_channel = nullptr,
            };
          }));

  for (const auto& discovered_medium : allowed_mediums) {
    pcp_handler.OnEndpointFound(
        &client,
        std::make_shared<MockDiscoveredEndpoint>(MockDiscoveredEndpoint{
            {
                std::string(kTestEndpointId),
                info.endpoint_info,
                "service",
                discovered_medium,
                WebRtcState::kUndefined,
            },
            MockContext{nullptr},
        }));
  }
  Status expected_result = {Status::kError};
  EXPECT_EQ(pcp_handler.RequestConnection(&client, std::string(kTestEndpointId),
                                          info, connection_options),
            expected_result);
  NEARBY_LOG(INFO, "RequestConnection complete");
  channel_b->Close();
  bwu.Shutdown();
  pcp_handler.DisconnectFromEndpointManager();
  env_.Stop();
}

TEST_P(BasePcpHandlerTest, IoError_RequestConnectionV3Fails) {
  env_.Start();
  ClientProxy client;
  Mediums m;
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  BwuManager bwu(m, em, ecm, {}, {});
  MockPcpHandler pcp_handler(&m, &em, &ecm, &bwu);
  StartDiscovery(&client, &pcp_handler);
  auto mediums = pcp_handler.GetDiscoveryMediums(&client);
  auto connect_medium = mediums[mediums.size() - 1];
  auto channel_pair = SetupConnection(connect_medium);
  auto& channel_a = channel_pair.first;
  auto& channel_b = channel_pair.second;
  EXPECT_CALL(*channel_a, CloseImpl).Times(AtLeast(1));
  EXPECT_CALL(*channel_b, CloseImpl).Times(AtLeast(1));
  channel_b->broken_write_ = true;
  EXPECT_CALL(mock_connection_listener_.rejected_cb, Call).Times(AtLeast(0));
  RequestConnectionV3(mock_device_, std::move(channel_a), channel_b.get(),
                      &client, &pcp_handler, connect_medium, nullptr,
                      {Status::kEndpointIoError});
  NEARBY_LOG(INFO, "RequestConnectionV3 complete");
  channel_b->Close();
  bwu.Shutdown();
  pcp_handler.DisconnectFromEndpointManager();
  env_.Stop();
}

TEST_P(BasePcpHandlerTest, IoError_RequestConnectionFails) {
  env_.Start();
  std::string endpoint_id{"1234"};
  ClientProxy client;
  Mediums m;
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  BwuManager bwu(m, em, ecm, {}, {});
  MockPcpHandler pcp_handler(&m, &em, &ecm, &bwu);
  StartDiscovery(&client, &pcp_handler);
  auto mediums = pcp_handler.GetDiscoveryMediums(&client);
  auto connect_medium = mediums[mediums.size() - 1];
  auto channel_pair = SetupConnection(connect_medium);
  auto& channel_a = channel_pair.first;
  auto& channel_b = channel_pair.second;
  EXPECT_CALL(*channel_a, CloseImpl).Times(AtLeast(1));
  EXPECT_CALL(*channel_b, CloseImpl).Times(AtLeast(1));
  channel_b->broken_write_ = true;
  EXPECT_CALL(mock_connection_listener_.rejected_cb, Call).Times(AtLeast(0));
  RequestConnection(endpoint_id, std::move(channel_a), channel_b.get(), &client,
                    &pcp_handler, connect_medium, nullptr,
                    {Status::kEndpointIoError});
  NEARBY_LOG(INFO, "RequestConnection complete");
  channel_b->Close();
  bwu.Shutdown();
  pcp_handler.DisconnectFromEndpointManager();
  env_.Stop();
}

TEST_P(BasePcpHandlerTest, AcceptConnectionChangesState) {
  env_.Start();
  std::string endpoint_id{"1234"};
  ClientProxy client;
  Mediums m;
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  BwuManager bwu(m, em, ecm, {}, {});
  MockPcpHandler pcp_handler(&m, &em, &ecm, &bwu);
  StartDiscovery(&client, &pcp_handler);
  auto mediums = pcp_handler.GetDiscoveryMediums(&client);
  auto connect_medium = mediums[mediums.size() - 1];
  auto channel_pair = SetupConnection(connect_medium);
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
  bwu.Shutdown();
  pcp_handler.DisconnectFromEndpointManager();
  env_.Stop();
}

TEST_P(BasePcpHandlerTest, RejectConnectionChangesState) {
  env_.Start();
  std::string endpoint_id{"1234"};
  ClientProxy client;
  Mediums m;
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  BwuManager bwu(m, em, ecm, {}, {});
  MockPcpHandler pcp_handler(&m, &em, &ecm, &bwu);
  StartDiscovery(&client, &pcp_handler);
  auto mediums = pcp_handler.GetDiscoveryMediums(&client);
  auto connect_medium = mediums[mediums.size() - 1];
  auto channel_pair = SetupConnection(connect_medium);
  auto& channel_b = channel_pair.second;
  EXPECT_CALL(mock_connection_listener_.rejected_cb, Call).Times(1);
  RequestConnection(endpoint_id, std::move(channel_pair.first), channel_b.get(),
                    &client, &pcp_handler, connect_medium);
  NEARBY_LOGS(INFO) << "Attempting to reject connection: id=" << endpoint_id;
  EXPECT_EQ(pcp_handler.RejectConnection(&client, endpoint_id),
            Status{Status::kSuccess});
  NEARBY_LOGS(INFO) << "Closing connection: id=" << endpoint_id;
  channel_b->Close();
  bwu.Shutdown();
  pcp_handler.DisconnectFromEndpointManager();
  env_.Stop();
}

TEST_P(BasePcpHandlerTest, OnIncomingFrameChangesState) {
  env_.Start();
  std::string endpoint_id{"1234"};
  ClientProxy client;
  Mediums m;
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  BwuManager bwu(m, em, ecm, {}, {});
  MockPcpHandler pcp_handler(&m, &em, &ecm, &bwu);
  analytics::PacketMetaData packet_meta_data;
  StartDiscovery(&client, &pcp_handler);
  auto mediums = pcp_handler.GetDiscoveryMediums(&client);
  auto connect_medium = mediums[mediums.size() - 1];
  auto channel_pair = SetupConnection(connect_medium);
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
  OsInfo os_info;
  auto frame = parser::FromBytes(
      parser::ForConnectionResponse(Status::kSuccess, os_info));
  EXPECT_CALL(mock_connection_listener_.bandwidth_changed_cb, Call).Times(1);
  pcp_handler.OnIncomingFrame(frame.result(), endpoint_id, &client,
                              connect_medium, packet_meta_data);
  NEARBY_LOGS(INFO) << "Closing connection: id=" << endpoint_id;
  channel_b->Close();
  bwu.Shutdown();
  pcp_handler.DisconnectFromEndpointManager();
  env_.Stop();
}

TEST_P(BasePcpHandlerTest, DestructorIsCalledOnProtocolEndpoint) {
  env_.Start();
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
    auto mediums = pcp_handler.GetDiscoveryMediums(&client);
    auto connect_medium = mediums[mediums.size() - 1];
    auto channel_pair = SetupConnection(connect_medium);
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
    bwu.Shutdown();
    pcp_handler.DisconnectFromEndpointManager();
  }
  EXPECT_EQ(destroyed_flag.load(), mediums_count);
  env_.Stop();
}

TEST_P(BasePcpHandlerTest, MultipleMediumsProduceSingleEndpointLostEvent) {
  env_.Start();
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
    auto mediums = pcp_handler.GetDiscoveryMediums(&client);
    auto connect_medium = mediums[mediums.size() - 1];
    auto channel_pair = SetupConnection(connect_medium);
    auto& channel_a = channel_pair.first;
    auto& channel_b = channel_pair.second;
    EXPECT_CALL(*channel_a, CloseImpl).Times(1);
    EXPECT_CALL(*channel_b, CloseImpl).Times(1);
    EXPECT_CALL(mock_discovery_listener_.endpoint_lost_cb, Call).Times(1);
    RequestConnection(endpoint_id, std::move(channel_a), channel_b.get(),
                      &client, &pcp_handler, connect_medium, &destroyed_flag);
    auto allowed_mediums = pcp_handler.GetDiscoveryMediums(&client);
    mediums_count = allowed_mediums.size();
    NEARBY_LOG(INFO, "Attempting to accept connection: id=%s",
               endpoint_id.c_str());
    EXPECT_EQ(pcp_handler.AcceptConnection(&client, endpoint_id, {}),
              Status{Status::kSuccess});
    EXPECT_CALL(mock_connection_listener_.rejected_cb, Call).Times(AtLeast(0));
    auto endpoint_disc = pcp_handler.GetDiscoveredEndpoint(endpoint_id);
    pcp_handler.OnEndpointLost(&client, *endpoint_disc);
    EXPECT_NE(pcp_handler.GetDiscoveredEndpoint(endpoint_id), nullptr);
    for (const auto* endpoint :
         pcp_handler.GetDiscoveredEndpoints(endpoint_id)) {
      pcp_handler.OnEndpointLost(&client, *endpoint);
    }
    EXPECT_EQ(pcp_handler.GetDiscoveredEndpoint(endpoint_id), nullptr);
    EXPECT_FALSE(client.IsConnectedToEndpoint(endpoint_id));
    NEARBY_LOG(INFO, "Closing connection: id=%s", endpoint_id.c_str());
    channel_b->Close();
    bwu.Shutdown();
    pcp_handler.DisconnectFromEndpointManager();
  }
  EXPECT_EQ(destroyed_flag.load(), mediums_count);
  env_.Stop();
}

INSTANTIATE_TEST_SUITE_P(ParameterizedBasePcpHandlerTest, BasePcpHandlerTest,
                         ::testing::ValuesIn(kTestCases));

TEST_F(BasePcpHandlerTest, InjectEndpoint) {
  env_.Start();
  std::string service_id{"service"};
  std::string endpoint_id{"ABCD"};
  ClientProxy client;
  Mediums m;
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  BwuManager bwu(m, em, ecm, {}, {});
  MockPcpHandler pcp_handler(&m, &em, &ecm, &bwu);
  BooleanMediumSelector allowed{
      .bluetooth = true,
  };
  DiscoveryOptions discovery_options{
      {
          Strategy::kP2pPointToPoint,
          allowed,
      },
      false,  // auto_upgrade_bandwidth;
      false,  // enforce_topology_constraints;
  };
  EXPECT_CALL(mock_discovery_listener_.endpoint_found_cb, Call);
  EXPECT_CALL(pcp_handler, StartDiscoveryImpl(&client, service_id, _))
      .WillOnce(Return(MockPcpHandler::StartOperationResult{
          .status = {Status::kSuccess},
          .mediums = allowed.GetMediums(true),
      }));
  EXPECT_EQ(pcp_handler.StartDiscovery(&client, service_id, discovery_options,
                                       GetDiscoveryListener()),
            Status{Status::kSuccess});
  EXPECT_TRUE(client.IsDiscovering());

  EXPECT_CALL(pcp_handler, InjectEndpointImpl(&client, service_id, _))
      .WillOnce(Invoke([&pcp_handler, &endpoint_id](
                           ClientProxy* client, const std::string& service_id,
                           const OutOfBandConnectionMetadata& metadata) {
        pcp_handler.OnEndpointFound(
            client,
            std::make_shared<MockDiscoveredEndpoint>(MockDiscoveredEndpoint{
                {
                    endpoint_id,
                    /*endpoint_info=*/ByteArray{"ABCD"},
                    service_id,
                    Medium::BLUETOOTH,
                    WebRtcState::kUndefined,
                },
                MockContext{nullptr},
            }));
        return Status{Status::kSuccess};
      }));
  pcp_handler.InjectEndpoint(
      &client, service_id,
      OutOfBandConnectionMetadata{
          .medium = Medium::BLUETOOTH,
          .remote_bluetooth_mac_address = ByteArray(kFakeMacAddress),
      });
  bwu.Shutdown();
  env_.Stop();
}

TEST_F(BasePcpHandlerTest,
       TestEndpointInfoChangedWhenEndpointDiscoveredOnMultipleMediums) {
  env_.Start();
  std::string service_id{"service"};
  std::string endpoint_id{"ABCD"};
  ClientProxy client;
  Mediums m;
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  BwuManager bwu(m, em, ecm, {}, {});
  MockPcpHandler pcp_handler(&m, &em, &ecm, &bwu);
  BooleanMediumSelector allowed{
      .bluetooth = true,
      .ble = true,
  };
  DiscoveryOptions discovery_options{
      {
          Strategy::kP2pPointToPoint,
          allowed,
      },
      false,  // auto_upgrade_bandwidth;
      false,  // enforce_topology_constraints;
  };

  EXPECT_CALL(pcp_handler, StartDiscoveryImpl)
      .WillOnce(Return(MockPcpHandler::StartOperationResult{
          .status = {Status::kSuccess},
          .mediums = allowed.GetMediums(true),
      }));

  EXPECT_EQ(pcp_handler.StartDiscovery(&client, service_id, discovery_options,
                                       GetDiscoveryListener()),
            Status{Status::kSuccess});
  EXPECT_TRUE(client.IsDiscovering());

  ::testing::InSequence seq;
  EXPECT_CALL(mock_discovery_listener_.endpoint_found_cb, Call)
      .WillOnce(Invoke([id = endpoint_id](const std::string& endpoint_id,
                                          const ByteArray& endpoint_info,
                                          const std::string& service_id) {
        EXPECT_EQ(endpoint_id, id);
        EXPECT_EQ(endpoint_info, ByteArray{"ABCD"});
      }));

  EXPECT_CALL(mock_discovery_listener_.endpoint_lost_cb, Call)
      .WillOnce(Invoke([id = endpoint_id](const std::string& endpoint_id) {
        EXPECT_EQ(endpoint_id, id);
      }));

  EXPECT_CALL(mock_discovery_listener_.endpoint_found_cb, Call)
      .WillOnce(Invoke([id = endpoint_id](const std::string& endpoint_id,
                                          const ByteArray& endpoint_info,
                                          const std::string& service_id) {
        EXPECT_EQ(endpoint_id, id);
        EXPECT_EQ(endpoint_info, ByteArray{"ABCDEF"});
      }));

  EXPECT_CALL(mock_discovery_listener_.endpoint_lost_cb, Call)
      .WillOnce(Invoke([id = endpoint_id](const std::string& endpoint_id) {
        EXPECT_EQ(endpoint_id, id);
      }));

  // Found endpoint on Bluetooth
  pcp_handler.OnEndpointFound(
      &client, std::make_shared<MockDiscoveredEndpoint>(MockDiscoveredEndpoint{
                   {
                       endpoint_id,
                       /*endpoint_info=*/ByteArray{"ABCD"},
                       service_id,
                       Medium::BLUETOOTH,
                       WebRtcState::kUndefined,
                   },
                   MockContext{nullptr},
               }));
  // Found endpoint on BLE
  pcp_handler.OnEndpointFound(
      &client, std::make_shared<MockDiscoveredEndpoint>(MockDiscoveredEndpoint{
                   {
                       endpoint_id,
                       /*endpoint_info=*/ByteArray{"ABCD"},
                       service_id,
                       Medium::BLE,
                       WebRtcState::kUndefined,
                   },
                   MockContext{nullptr},
               }));

  // Endpoint info changed on BLE
  pcp_handler.OnEndpointFound(
      &client, std::make_shared<MockDiscoveredEndpoint>(MockDiscoveredEndpoint{
                   {
                       endpoint_id,
                       /*endpoint_info=*/ByteArray{"ABCDEF"},
                       service_id,
                       Medium::BLE,
                       WebRtcState::kUndefined,
                   },
                   MockContext{nullptr},
               }));

  pcp_handler.OnEndpointLost(&client,
                             MockDiscoveredEndpoint{
                                 {
                                     endpoint_id,
                                     /*endpoint_info=*/ByteArray{"ABCDEF"},
                                     service_id,
                                     Medium::BLE,
                                     WebRtcState::kUndefined,
                                 },
                                 MockContext{nullptr},
                             });

  env_.Sync(false);
  env_.Stop();
}

TEST_F(BasePcpHandlerTest, TestStartStopEndpointLostAlarm) {
  env_.Start();
  std::string service_id{"service"};
  std::string endpoint_id{"ABCD"};
  ClientProxy client;
  Mediums m;
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  BwuManager bwu(m, em, ecm, {}, {});
  MockPcpHandler pcp_handler(&m, &em, &ecm, &bwu);
  BooleanMediumSelector allowed{
      .bluetooth = true,
  };
  DiscoveryOptions discovery_options{
      {
          Strategy::kP2pPointToPoint,
          allowed,
      },
      false,  // auto_upgrade_bandwidth;
      false,  // enforce_topology_constraints;
  };
  EXPECT_CALL(pcp_handler, StartDiscoveryImpl)
      .WillOnce(Return(MockPcpHandler::StartOperationResult{
          .status = {Status::kSuccess},
          .mediums = allowed.GetMediums(true),
      }));
  EXPECT_EQ(
      pcp_handler.StartDiscovery(&client, service_id, discovery_options, {}),
      Status{Status::kSuccess});
  EXPECT_TRUE(client.IsDiscovering());

  EXPECT_CALL(pcp_handler, InjectEndpointImpl)
      .WillOnce(Invoke([&pcp_handler, &endpoint_id](
                           ClientProxy* client, const std::string& service_id,
                           const OutOfBandConnectionMetadata& metadata) {
        pcp_handler.OnEndpointFound(
            client,
            std::make_shared<MockDiscoveredEndpoint>(MockDiscoveredEndpoint{
                {
                    endpoint_id,
                    /*endpoint_info=*/ByteArray{"ABCD"},
                    service_id,
                    Medium::BLUETOOTH,
                    WebRtcState::kUndefined,
                },
                MockContext{nullptr},
            }));
        return Status{Status::kSuccess};
      }));
  pcp_handler.InjectEndpoint(
      &client, service_id,
      OutOfBandConnectionMetadata{
          .medium = Medium::BLUETOOTH,
          .remote_bluetooth_mac_address = ByteArray(kFakeMacAddress),
      });
  EXPECT_EQ(pcp_handler.GetDiscoveredEndpoints(Medium::BLUETOOTH).size(), 1);
  EXPECT_EQ(pcp_handler.GetEndpointLostByMediumAlarmsCount(), 0);
  pcp_handler.StartEndpointLostByMediumAlarms(&client, Medium::BLUETOOTH);
  EXPECT_EQ(pcp_handler.GetEndpointLostByMediumAlarmsCount(), 1);
  pcp_handler.StopEndpointLostByMediumAlarm(endpoint_id, Medium::BLUETOOTH);
  EXPECT_EQ(pcp_handler.GetEndpointLostByMediumAlarmsCount(), 0);
  env_.Stop();
}

TEST_F(BasePcpHandlerTest, TestStartEndpointLostByMediumAlarms) {
  env_.Start();
  std::string service_id{"service"};
  std::string endpoint_id{"ABCD"};
  ClientProxy client;
  Mediums m;
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  BwuManager bwu(m, em, ecm, {}, {});
  MockPcpHandler pcp_handler(&m, &em, &ecm, &bwu);
  BooleanMediumSelector allowed{
      .bluetooth = true,
  };
  DiscoveryOptions discovery_options{
      {
          Strategy::kP2pPointToPoint,
          allowed,
      },
      false,  // auto_upgrade_bandwidth;
      false,  // enforce_topology_constraints;
  };
  EXPECT_CALL(pcp_handler, StartDiscoveryImpl)
      .WillOnce(Return(MockPcpHandler::StartOperationResult{
          .status = {Status::kSuccess},
          .mediums = allowed.GetMediums(true),
      }));
  EXPECT_EQ(
      pcp_handler.StartDiscovery(&client, service_id, discovery_options, {}),
      Status{Status::kSuccess});
  EXPECT_TRUE(client.IsDiscovering());

  EXPECT_CALL(pcp_handler, InjectEndpointImpl)
      .WillOnce(Invoke([&pcp_handler, &endpoint_id](
                           ClientProxy* client, const std::string& service_id,
                           const OutOfBandConnectionMetadata& metadata) {
        pcp_handler.OnEndpointFound(
            client,
            std::make_shared<MockDiscoveredEndpoint>(MockDiscoveredEndpoint{
                {
                    endpoint_id,
                    /*endpoint_info=*/ByteArray{"ABCD"},
                    service_id,
                    Medium::BLUETOOTH,
                    WebRtcState::kUndefined,
                },
                MockContext{nullptr},
            }));
        return Status{Status::kSuccess};
      }));
  pcp_handler.InjectEndpoint(
      &client, service_id,
      OutOfBandConnectionMetadata{
          .medium = Medium::BLUETOOTH,
          .remote_bluetooth_mac_address = ByteArray(kFakeMacAddress),
      });
  EXPECT_EQ(pcp_handler.GetDiscoveredEndpoints(Medium::BLUETOOTH).size(), 1);
  EXPECT_EQ(pcp_handler.GetEndpointLostByMediumAlarmsCount(), 0);
  pcp_handler.StartEndpointLostByMediumAlarms(&client, Medium::BLUETOOTH);
  EXPECT_EQ(pcp_handler.GetEndpointLostByMediumAlarmsCount(), 1);
  absl::SleepFor(absl::Seconds(11));
  EXPECT_EQ(pcp_handler.GetDiscoveredEndpoints(Medium::BLUETOOTH).size(), 0);
  EXPECT_EQ(pcp_handler.GetEndpointLostByMediumAlarmsCount(), 0);
  env_.Stop();
}

TEST_F(BasePcpHandlerTest, TestEndpointFoundStopsAlarm) {
  env_.Start();
  std::string service_id{"service"};
  std::string endpoint_id{"ABCD"};
  ClientProxy client;
  Mediums m;
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  BwuManager bwu(m, em, ecm, {}, {});
  MockPcpHandler pcp_handler(&m, &em, &ecm, &bwu);
  BooleanMediumSelector allowed{
      .bluetooth = true,
  };
  DiscoveryOptions discovery_options{
      {
          Strategy::kP2pPointToPoint,
          allowed,
      },
      false,  // auto_upgrade_bandwidth;
      false,  // enforce_topology_constraints;
  };
  EXPECT_CALL(pcp_handler, StartDiscoveryImpl)
      .WillOnce(Return(MockPcpHandler::StartOperationResult{
          .status = {Status::kSuccess},
          .mediums = allowed.GetMediums(true),
      }));
  EXPECT_EQ(
      pcp_handler.StartDiscovery(&client, service_id, discovery_options, {}),
      Status{Status::kSuccess});
  EXPECT_TRUE(client.IsDiscovering());

  bool first_call = true;
  EXPECT_CALL(pcp_handler, InjectEndpointImpl)
      .Times(2)
      .WillRepeatedly(
          Invoke([&pcp_handler, &endpoint_id, &first_call](
                     ClientProxy* client, const std::string& service_id,
                     const OutOfBandConnectionMetadata& metadata) {
            ByteArray endpoint_info;
            if (first_call) {
              endpoint_info = ByteArray("ABCD");
            } else {
              endpoint_info = ByteArray("ABCDE");
            }
            first_call = false;
            pcp_handler.OnEndpointFound(
                client,
                std::make_shared<MockDiscoveredEndpoint>(MockDiscoveredEndpoint{
                    {
                        endpoint_id,
                        endpoint_info,
                        service_id,
                        Medium::BLUETOOTH,
                        WebRtcState::kUndefined,
                    },
                    MockContext{nullptr},
                }));
            return Status{Status::kSuccess};
          }));
  pcp_handler.InjectEndpoint(
      &client, service_id,
      OutOfBandConnectionMetadata{
          .medium = Medium::BLUETOOTH,
          .remote_bluetooth_mac_address = ByteArray(kFakeMacAddress),
      });
  EXPECT_EQ(pcp_handler.GetDiscoveredEndpoints(Medium::BLUETOOTH).size(), 1);
  EXPECT_EQ(pcp_handler.GetEndpointLostByMediumAlarmsCount(), 0);
  pcp_handler.StartEndpointLostByMediumAlarms(&client, Medium::BLUETOOTH);
  EXPECT_EQ(pcp_handler.GetEndpointLostByMediumAlarmsCount(), 1);
  pcp_handler.InjectEndpoint(
      &client, service_id,
      OutOfBandConnectionMetadata{
          .medium = Medium::BLUETOOTH,
          .remote_bluetooth_mac_address = ByteArray(kFakeMacAddress),
      });
  EXPECT_EQ(pcp_handler.GetEndpointLostByMediumAlarmsCount(), 0);
  env_.Stop();
}

TEST_P(BasePcpHandlerTest, TestGetConnectionInfosFromMediums) {
  env_.Start();
  Mediums mediums;
  EndpointChannelManager endpoint_channel_manager;
  EndpointManager endpoint_manager(&endpoint_channel_manager);
  BwuManager bwu_manager(mediums, endpoint_manager, endpoint_channel_manager,
                         {}, {});
  MockPcpHandler pcp_handler(&mediums, &endpoint_manager,
                             &endpoint_channel_manager, &bwu_manager);
  BooleanMediumSelector selector = GetParam();
  // Flip on a medium we should not get info for.
  selector.web_rtc = true;
  std::vector<ConnectionInfoVariant> infos =
      pcp_handler.GetConnectionInfoFromResult(
          "service", {.mediums = selector.GetMediums(true)});
  // Make sure we don't count webrtc.
  EXPECT_EQ(infos.size(), selector.Count(true) - 1);
  env_.Stop();
}

TEST_F(BasePcpHandlerTest, TestCanStartListeningForIncomingConnections) {
  env_.Start();
  ClientProxy client;
  Mediums mediums;
  EndpointChannelManager endpoint_channel_manager;
  EndpointManager endpoint_manager(&endpoint_channel_manager);
  BwuManager bwu_manager(mediums, endpoint_manager, endpoint_channel_manager,
                         {}, {});
  MockPcpHandler pcp_handler(&mediums, &endpoint_manager,
                             &endpoint_channel_manager, &bwu_manager);
  EXPECT_CALL(pcp_handler, StartListeningForIncomingConnectionsImpl)
      .Times(1)
      .WillOnce(Return(
          MockPcpHandler::StartOperationResult{.status = {Status::kSuccess}}));
  v3::ConnectionListeningOptions options = {.strategy = Strategy::kP2pCluster,
                                            .enable_ble_listening = true,
                                            .enable_bluetooth_listening = true,
                                            .enable_wlan_listening = true};
  pcp_handler.StartListeningForIncomingConnections(&client, "service", options,
                                                   {});
  EXPECT_TRUE(client.IsListeningForIncomingConnections());
}

TEST_F(BasePcpHandlerTest, TestStartListeningForIncomingConnectionsBadStatus) {
  env_.Start();
  ClientProxy client;
  Mediums m;
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  BwuManager bwu(m, em, ecm, {}, {});
  MockPcpHandler pcp_handler(&m, &em, &ecm, &bwu);
  EXPECT_CALL(pcp_handler, StartListeningForIncomingConnectionsImpl)
      .Times(1)
      .WillOnce(Return(MockPcpHandler::StartOperationResult{
          .status = {Status::kAlreadyListening}}));
  v3::ConnectionListeningOptions options = {.strategy = Strategy::kP2pCluster,
                                            .enable_ble_listening = true,
                                            .enable_bluetooth_listening = true,
                                            .enable_wlan_listening = true};
  pcp_handler.StartListeningForIncomingConnections(&client, "service", options,
                                                   {});
  EXPECT_FALSE(client.IsListeningForIncomingConnections());
}

TEST_F(BasePcpHandlerTest, TestCanStopListeningForIncomingConnections) {
  env_.Start();
  ClientProxy client;
  Mediums m;
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  BwuManager bwu(m, em, ecm, {}, {});
  MockPcpHandler pcp_handler(&m, &em, &ecm, &bwu);
  EXPECT_CALL(pcp_handler, StartListeningForIncomingConnectionsImpl)
      .Times(1)
      .WillOnce(Return(
          MockPcpHandler::StartOperationResult{.status = {Status::kSuccess}}));
  EXPECT_CALL(pcp_handler, StopListeningForIncomingConnectionsImpl).Times(1);
  v3::ConnectionListeningOptions options = {.strategy = Strategy::kP2pCluster,
                                            .enable_ble_listening = true,
                                            .enable_bluetooth_listening = true,
                                            .enable_wlan_listening = true};
  pcp_handler.StartListeningForIncomingConnections(&client, "service", options,
                                                   {});
  pcp_handler.StopListeningForIncomingConnections(&client);
  EXPECT_FALSE(client.IsListeningForIncomingConnections());
}

TEST_F(BasePcpHandlerTest,
       TestWifiLanStopListeningForIncomingConnectionsSuccessWhenStopped) {
  env_.Start();
  ClientProxy client;
  Mediums m;
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  BwuManager bwu(m, em, ecm, {}, {});
  MockPcpHandler pcp_handler(&m, &em, &ecm, &bwu);
  EXPECT_CALL(pcp_handler, StartListeningForIncomingConnectionsImpl)
      .Times(1)
      .WillOnce(Return(
          MockPcpHandler::StartOperationResult{.status = {Status::kSuccess}}));
  EXPECT_CALL(pcp_handler, StopListeningForIncomingConnectionsImpl).Times(1);
  v3::ConnectionListeningOptions options = {.strategy = Strategy::kP2pCluster,
                                            .enable_ble_listening = true,
                                            .enable_bluetooth_listening = true,
                                            .enable_wlan_listening = true};
  pcp_handler.StartListeningForIncomingConnections(&client, "service", options,
                                                   {});
  m.GetWifiLan().StopAcceptingConnections("service");
  pcp_handler.StopListeningForIncomingConnections(&client);
  EXPECT_FALSE(client.IsListeningForIncomingConnections());
}

TEST_F(BasePcpHandlerTest, TestDeviceFilterForConnectionsWithUnknown) {
  env_.Start();
  ClientProxy client;
  Mediums m;
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  BwuManager bwu(m, em, ecm, {}, {});
  MockPcpHandler pcp_handler(&m, &em, &ecm, &bwu);
  v3::ConnectionListeningOptions options = {
      .strategy = Strategy::kP2pCluster,
      .enable_ble_listening = true,
      .enable_bluetooth_listening = true,
      .enable_wlan_listening = true,
      .listening_endpoint_type = NearbyDevice::Type::kConnectionsDevice};
  EXPECT_CALL(pcp_handler, StartListeningForIncomingConnectionsImpl)
      .WillOnce(Return(
          MockPcpHandler::StartOperationResult{.status = {Status::kSuccess}}));
  EXPECT_CALL(pcp_handler, CanReceiveIncomingConnection)
      .WillRepeatedly(Return(true));
  EXPECT_TRUE(
      pcp_handler
          .StartListeningForIncomingConnections(&client, "service", options, {})
          .first.Ok());
  ASSERT_TRUE(client.IsListeningForIncomingConnections());
  ASSERT_TRUE(pcp_handler.CanReceiveIncomingConnection(&client));
  auto channel_pair = SetupConnection(Medium::BLUETOOTH);
  ByteArray serialized_frame = parser::ForConnectionRequestConnections(
      {}, {
              .local_endpoint_id = "ABCD",
              .local_endpoint_info = ByteArray("local endpoint"),
          });
  location::nearby::connections::OfflineFrame frame;
  frame.ParseFromString(serialized_frame.AsStringView());
  frame.mutable_v1()->mutable_connection_request()->clear_connections_device();
  frame.mutable_v1()->mutable_connection_request()->clear_presence_device();
  ASSERT_FALSE(frame.v1().connection_request().has_connections_device());
  ASSERT_FALSE(frame.v1().connection_request().has_presence_device());
  // do a dummy write to get to the actual write.
  channel_pair.first->Write(ByteArray());
  channel_pair.first->Write(ByteArray(frame.SerializeAsString()));
  EXPECT_TRUE(pcp_handler
                  .OnIncomingConnection(&client, ByteArray("remote endpoint"),
                                        std::move(channel_pair.second),
                                        Medium::BLUETOOTH,
                                        NearbyDevice::Type::kConnectionsDevice)
                  .Ok());
  env_.Stop();
}

TEST_F(BasePcpHandlerTest, TestDeviceFilterForPresenceWithUnknown) {
  env_.Start();
  ClientProxy client;
  Mediums m;
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  BwuManager bwu(m, em, ecm, {}, {});
  MockPcpHandler pcp_handler(&m, &em, &ecm, &bwu);
  v3::ConnectionListeningOptions options = {
      .strategy = Strategy::kP2pCluster,
      .enable_ble_listening = true,
      .enable_bluetooth_listening = true,
      .enable_wlan_listening = true,
      .listening_endpoint_type = NearbyDevice::Type::kPresenceDevice};
  EXPECT_CALL(pcp_handler, StartListeningForIncomingConnectionsImpl)
      .WillOnce(Return(
          MockPcpHandler::StartOperationResult{.status = {Status::kSuccess}}));
  EXPECT_CALL(pcp_handler, CanReceiveIncomingConnection)
      .WillRepeatedly(Return(true));
  EXPECT_TRUE(
      pcp_handler
          .StartListeningForIncomingConnections(&client, "service", options, {})
          .first.Ok());
  ASSERT_TRUE(client.IsListeningForIncomingConnections());
  ASSERT_TRUE(pcp_handler.CanReceiveIncomingConnection(&client));
  auto channel_pair = SetupConnection(Medium::BLUETOOTH);
  ByteArray serialized_frame = parser::ForConnectionRequestConnections(
      {}, {
              .local_endpoint_id = "ABCD",
              .local_endpoint_info = ByteArray("local endpoint"),
          });
  location::nearby::connections::OfflineFrame frame;
  frame.ParseFromString(serialized_frame.AsStringView());
  frame.mutable_v1()->mutable_connection_request()->clear_connections_device();
  frame.mutable_v1()->mutable_connection_request()->clear_presence_device();
  ASSERT_FALSE(frame.v1().connection_request().has_connections_device());
  ASSERT_FALSE(frame.v1().connection_request().has_presence_device());
  // do a dummy write to get to the actual write.
  channel_pair.first->Write(ByteArray());
  channel_pair.first->Write(ByteArray(frame.SerializeAsString()));
  EXPECT_EQ(pcp_handler
                .OnIncomingConnection(&client, ByteArray("remote endpoint"),
                                      std::move(channel_pair.second),
                                      Medium::BLUETOOTH,
                                      NearbyDevice::Type::kPresenceDevice)
                .value,
            Exception::Value::kIo);
  env_.Stop();
}

TEST_F(BasePcpHandlerTest, TestDeviceFilterForPresenceWithConnections) {
  env_.Start();
  ClientProxy client;
  Mediums m;
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  BwuManager bwu(m, em, ecm, {}, {});
  MockPcpHandler pcp_handler(&m, &em, &ecm, &bwu);
  v3::ConnectionListeningOptions options = {
      .strategy = Strategy::kP2pCluster,
      .enable_ble_listening = true,
      .enable_bluetooth_listening = true,
      .enable_wlan_listening = true,
      .listening_endpoint_type = NearbyDevice::Type::kPresenceDevice};
  EXPECT_CALL(pcp_handler, StartListeningForIncomingConnectionsImpl)
      .WillOnce(Return(
          MockPcpHandler::StartOperationResult{.status = {Status::kSuccess}}));
  EXPECT_CALL(pcp_handler, CanReceiveIncomingConnection)
      .WillRepeatedly(Return(true));
  EXPECT_TRUE(
      pcp_handler
          .StartListeningForIncomingConnections(&client, "service", options, {})
          .first.Ok());
  ASSERT_TRUE(client.IsListeningForIncomingConnections());
  ASSERT_TRUE(pcp_handler.CanReceiveIncomingConnection(&client));
  auto channel_pair = SetupConnection(Medium::BLUETOOTH);
  ByteArray serialized_frame = parser::ForConnectionRequestConnections(
      {}, {
              .local_endpoint_id = "ABCD",
              .local_endpoint_info = ByteArray("local endpoint"),
          });
  location::nearby::connections::OfflineFrame frame;
  frame.ParseFromString(serialized_frame.AsStringView());
  frame.mutable_v1()
      ->mutable_connection_request()
      ->mutable_connections_device()
      ->set_endpoint_id("ABCD");
  ASSERT_TRUE(frame.v1().connection_request().has_connections_device());
  // do a dummy write to get to the actual write.
  channel_pair.first->Write(ByteArray());
  channel_pair.first->Write(ByteArray(frame.SerializeAsString()));
  EXPECT_EQ(pcp_handler
                .OnIncomingConnection(&client, ByteArray("remote endpoint"),
                                      std::move(channel_pair.second),
                                      Medium::BLUETOOTH,
                                      NearbyDevice::Type::kPresenceDevice)
                .value,
            Exception::Value::kIo);
  env_.Stop();
}

TEST_F(BasePcpHandlerTest, TestDeviceFilterForPresenceWithPresence) {
  env_.Start();
  ClientProxy client;
  Mediums m;
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  BwuManager bwu(m, em, ecm, {}, {});
  MockPcpHandler pcp_handler(&m, &em, &ecm, &bwu);
  v3::ConnectionListeningOptions options = {
      .strategy = Strategy::kP2pCluster,
      .enable_ble_listening = true,
      .enable_bluetooth_listening = true,
      .enable_wlan_listening = true,
      .listening_endpoint_type = NearbyDevice::Type::kPresenceDevice};
  EXPECT_CALL(pcp_handler, StartListeningForIncomingConnectionsImpl)
      .WillOnce(Return(
          MockPcpHandler::StartOperationResult{.status = {Status::kSuccess}}));
  EXPECT_CALL(pcp_handler, CanReceiveIncomingConnection)
      .WillRepeatedly(Return(true));
  EXPECT_TRUE(
      pcp_handler
          .StartListeningForIncomingConnections(&client, "service", options, {})
          .first.Ok());
  ASSERT_TRUE(client.IsListeningForIncomingConnections());
  ASSERT_TRUE(pcp_handler.CanReceiveIncomingConnection(&client));
  auto channel_pair = SetupConnection(Medium::BLUETOOTH);
  ByteArray serialized_frame = parser::ForConnectionRequestConnections(
      {}, {
              .local_endpoint_id = "ABCD",
              .local_endpoint_info = ByteArray("local endpoint"),
          });
  location::nearby::connections::OfflineFrame frame;
  frame.ParseFromString(serialized_frame.AsStringView());
  frame.mutable_v1()
      ->mutable_connection_request()
      ->mutable_presence_device()
      ->set_endpoint_id("ABCD");
  ASSERT_TRUE(frame.v1().connection_request().has_presence_device());
  // do a dummy write to get to the actual write.
  channel_pair.first->Write(ByteArray());
  channel_pair.first->Write(ByteArray(frame.SerializeAsString()));
  EXPECT_TRUE(pcp_handler
                  .OnIncomingConnection(&client, ByteArray("remote endpoint"),
                                        std::move(channel_pair.second),
                                        Medium::BLUETOOTH,
                                        NearbyDevice::Type::kPresenceDevice)
                  .Ok());
  env_.Stop();
}

TEST_F(BasePcpHandlerTest, TestDeviceFilterForConnectionsWithConnections) {
  env_.Start();
  ClientProxy client;
  Mediums m;
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  BwuManager bwu(m, em, ecm, {}, {});
  MockPcpHandler pcp_handler(&m, &em, &ecm, &bwu);
  v3::ConnectionListeningOptions options = {
      .strategy = Strategy::kP2pCluster,
      .enable_ble_listening = true,
      .enable_bluetooth_listening = true,
      .enable_wlan_listening = true,
      .listening_endpoint_type = NearbyDevice::Type::kConnectionsDevice};
  EXPECT_CALL(pcp_handler, StartListeningForIncomingConnectionsImpl)
      .WillOnce(Return(
          MockPcpHandler::StartOperationResult{.status = {Status::kSuccess}}));
  EXPECT_CALL(pcp_handler, CanReceiveIncomingConnection)
      .WillRepeatedly(Return(true));
  EXPECT_TRUE(
      pcp_handler
          .StartListeningForIncomingConnections(&client, "service", options, {})
          .first.Ok());
  ASSERT_TRUE(client.IsListeningForIncomingConnections());
  ASSERT_TRUE(pcp_handler.CanReceiveIncomingConnection(&client));
  auto channel_pair = SetupConnection(Medium::BLUETOOTH);
  ByteArray serialized_frame = parser::ForConnectionRequestConnections(
      {}, {
              .local_endpoint_id = "ABCD",
              .local_endpoint_info = ByteArray("local endpoint"),
          });
  location::nearby::connections::OfflineFrame frame;
  frame.ParseFromString(serialized_frame.AsStringView());
  frame.mutable_v1()
      ->mutable_connection_request()
      ->mutable_connections_device()
      ->set_endpoint_id("ABCD");
  ASSERT_TRUE(frame.v1().connection_request().has_connections_device());
  // do a dummy write to get to the actual write.
  channel_pair.first->Write(ByteArray());
  channel_pair.first->Write(ByteArray(frame.SerializeAsString()));
  EXPECT_TRUE(pcp_handler
                  .OnIncomingConnection(&client, ByteArray("remote endpoint"),
                                        std::move(channel_pair.second),
                                        Medium::BLUETOOTH,
                                        NearbyDevice::Type::kConnectionsDevice)
                  .Ok());
  env_.Stop();
}

TEST_F(BasePcpHandlerTest, TestDeviceFilterForConnectionsWithPresence) {
  env_.Start();
  ClientProxy client;
  Mediums m;
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  BwuManager bwu(m, em, ecm, {}, {});
  MockPcpHandler pcp_handler(&m, &em, &ecm, &bwu);
  v3::ConnectionListeningOptions options = {
      .strategy = Strategy::kP2pCluster,
      .enable_ble_listening = true,
      .enable_bluetooth_listening = true,
      .enable_wlan_listening = true,
      .listening_endpoint_type = NearbyDevice::Type::kConnectionsDevice};
  EXPECT_CALL(pcp_handler, StartListeningForIncomingConnectionsImpl)
      .WillOnce(Return(
          MockPcpHandler::StartOperationResult{.status = {Status::kSuccess}}));
  EXPECT_CALL(pcp_handler, CanReceiveIncomingConnection)
      .WillRepeatedly(Return(true));
  EXPECT_TRUE(
      pcp_handler
          .StartListeningForIncomingConnections(&client, "service", options, {})
          .first.Ok());
  ASSERT_TRUE(client.IsListeningForIncomingConnections());
  ASSERT_TRUE(pcp_handler.CanReceiveIncomingConnection(&client));
  auto channel_pair = SetupConnection(Medium::BLUETOOTH);
  ByteArray serialized_frame = parser::ForConnectionRequestConnections(
      {}, {
              .local_endpoint_id = "ABCD",
              .local_endpoint_info = ByteArray("local endpoint"),
          });
  location::nearby::connections::OfflineFrame frame;
  frame.ParseFromString(serialized_frame.AsStringView());
  frame.mutable_v1()
      ->mutable_connection_request()
      ->mutable_presence_device()
      ->set_endpoint_id("ABCD");
  ASSERT_TRUE(frame.v1().connection_request().has_presence_device());
  // do a dummy write to get to the actual write.
  channel_pair.first->Write(ByteArray());
  channel_pair.first->Write(ByteArray(frame.SerializeAsString()));
  EXPECT_EQ(pcp_handler
                .OnIncomingConnection(&client, ByteArray("remote endpoint"),
                                      std::move(channel_pair.second),
                                      Medium::BLUETOOTH,
                                      NearbyDevice::Type::kConnectionsDevice)
                .value,
            Exception::Value::kIo);
  env_.Stop();
}

TEST_F(BasePcpHandlerTest, TestNeedsToTurnOffAdvertisingMedium) {
  Mediums m;
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  BwuManager bwu(m, em, ecm, {}, {});
  MockPcpHandler pcp_handler(&m, &em, &ecm, &bwu);
  BooleanMediumSelector old_meds{
      .bluetooth = true,
      .ble = true,
      .wifi_lan = false,
  };
  AdvertisingOptions old_opts, new_opts;
  old_opts.allowed = old_meds;
  BooleanMediumSelector new_meds{
      .bluetooth = false,
      .ble = true,
      .wifi_lan = false,
  };
  new_opts.allowed = new_meds;
  EXPECT_TRUE(pcp_handler.NeedsToTurnOffAdvertisingMedium(Medium::BLUETOOTH,
                                                          old_opts, new_opts));
  EXPECT_FALSE(pcp_handler.NeedsToTurnOffAdvertisingMedium(Medium::BLE,
                                                           old_opts, new_opts));
  EXPECT_FALSE(pcp_handler.NeedsToTurnOffAdvertisingMedium(Medium::WIFI_LAN,
                                                           old_opts, new_opts));
}

TEST_F(BasePcpHandlerTest, TestUpdateAdvertisingOptionsWorks) {
  env_.Start();
  AdvertisingOptions old_options{
      {},
      true,   // auto_upgrade_bandwidth
      true,   // enforce_topology_constraints
      true,   // low_power
      false,  // enable_bluetooth_listening
  };
  AdvertisingOptions new_options{
      {},
      true,   // auto_upgrade_bandwidth
      true,   // enforce_topology_constraints
      false,  // low_power
      true,   // enable_bluetooth_listening
  };
  ClientProxy client;
  Mediums m;
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  BwuManager bwu(m, em, ecm, {}, {});
  MockPcpHandler pcp_handler(&m, &em, &ecm, &bwu);
  StartAdvertisingWithOptions(&client, &pcp_handler, old_options);
  EXPECT_TRUE(client.IsAdvertising());
  auto current_client_opts = client.GetAdvertisingOptions();
  // check custom option parameters
  EXPECT_EQ(current_client_opts.auto_upgrade_bandwidth,
            old_options.auto_upgrade_bandwidth);
  EXPECT_EQ(current_client_opts.enforce_topology_constraints,
            old_options.enforce_topology_constraints);
  EXPECT_EQ(current_client_opts.low_power, old_options.low_power);
  EXPECT_EQ(current_client_opts.enable_bluetooth_listening,
            old_options.enable_bluetooth_listening);
  UpdateAdvertisingOptions(&client, &pcp_handler, new_options,
                           {Status::kSuccess});
  EXPECT_TRUE(client.IsAdvertising());
  current_client_opts = client.GetAdvertisingOptions();
  // check new option parameters
  EXPECT_EQ(current_client_opts.auto_upgrade_bandwidth,
            new_options.auto_upgrade_bandwidth);
  EXPECT_EQ(current_client_opts.enforce_topology_constraints,
            new_options.enforce_topology_constraints);
  EXPECT_EQ(current_client_opts.low_power, new_options.low_power);
  EXPECT_EQ(current_client_opts.enable_bluetooth_listening,
            new_options.enable_bluetooth_listening);
  env_.Stop();
}

TEST_F(BasePcpHandlerTest, TestUpdateAdvertisingOptionsFailsWithBadStatus) {
  env_.Start();
  AdvertisingOptions old_options{
      {},
      true,   // auto_upgrade_bandwidth
      true,   // enforce_topology_constraints
      true,   // low_power
      false,  // enable_bluetooth_listening
  };
  AdvertisingOptions new_options{
      {},
      true,   // auto_upgrade_bandwidth
      true,   // enforce_topology_constraints
      false,  // low_power
      true,   // enable_bluetooth_listening
  };
  ClientProxy client;
  Mediums m;
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  BwuManager bwu(m, em, ecm, {}, {});
  MockPcpHandler pcp_handler(&m, &em, &ecm, &bwu);
  StartAdvertisingWithOptions(&client, &pcp_handler, old_options);
  EXPECT_TRUE(client.IsAdvertising());
  auto current_client_opts = client.GetAdvertisingOptions();
  // check custom option parameters
  EXPECT_EQ(current_client_opts.auto_upgrade_bandwidth,
            old_options.auto_upgrade_bandwidth);
  EXPECT_EQ(current_client_opts.enforce_topology_constraints,
            old_options.enforce_topology_constraints);
  EXPECT_EQ(current_client_opts.low_power, old_options.low_power);
  EXPECT_EQ(current_client_opts.enable_bluetooth_listening,
            old_options.enable_bluetooth_listening);
  UpdateAdvertisingOptions(&client, &pcp_handler, new_options,
                           {Status::kBleError});
  EXPECT_TRUE(client.IsAdvertising());
  current_client_opts = client.GetAdvertisingOptions();
  // check new option parameters
  EXPECT_EQ(current_client_opts.auto_upgrade_bandwidth,
            old_options.auto_upgrade_bandwidth);
  EXPECT_EQ(current_client_opts.enforce_topology_constraints,
            old_options.enforce_topology_constraints);
  EXPECT_EQ(current_client_opts.low_power, old_options.low_power);
  EXPECT_EQ(current_client_opts.enable_bluetooth_listening,
            old_options.enable_bluetooth_listening);
  env_.Stop();
}

TEST_F(BasePcpHandlerTest, TestNeedsToTurnOffDiscoveryMedium) {
  Mediums m;
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  BwuManager bwu(m, em, ecm, {}, {});
  MockPcpHandler pcp_handler(&m, &em, &ecm, &bwu);
  BooleanMediumSelector old_meds{
      .bluetooth = true,
      .ble = true,
      .wifi_lan = false,
  };
  DiscoveryOptions old_opts, new_opts;
  old_opts.allowed = old_meds;
  BooleanMediumSelector new_meds{
      .bluetooth = false,
      .ble = true,
      .wifi_lan = false,
  };
  new_opts.allowed = new_meds;
  EXPECT_TRUE(pcp_handler.NeedsToTurnOffDiscoveryMedium(Medium::BLUETOOTH,
                                                        old_opts, new_opts));
  EXPECT_FALSE(pcp_handler.NeedsToTurnOffDiscoveryMedium(Medium::BLE, old_opts,
                                                         new_opts));
  EXPECT_FALSE(pcp_handler.NeedsToTurnOffDiscoveryMedium(Medium::WIFI_LAN,
                                                         old_opts, new_opts));
}

TEST_F(BasePcpHandlerTest, TestUpdateDiscoveryOptionsWorks) {
  env_.Start();
  DiscoveryOptions old_options{
      {},
      true,   // auto_upgrade_bandwidth
      true,   // enforce_topology_constraints
      false,  // is_out_of_band_connection,
      "",     // fast_advertisement_service_uuid
      false,  // low_power
  };
  DiscoveryOptions new_options{
      {},
      true,   // auto_upgrade_bandwidth
      true,   // enforce_topology_constraints
      false,  // is_out_of_band_connection,
      "",     // fast_advertisement_service_uuid
      true,   // low_power
  };
  ClientProxy client;
  Mediums m;
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  BwuManager bwu(m, em, ecm, {}, {});
  MockPcpHandler pcp_handler(&m, &em, &ecm, &bwu);
  StartDiscoveryWithOptions(&client, &pcp_handler, old_options);
  EXPECT_TRUE(client.IsDiscovering());
  auto current_client_opts = client.GetDiscoveryOptions();
  // check custom option parameters
  EXPECT_EQ(current_client_opts.auto_upgrade_bandwidth,
            old_options.auto_upgrade_bandwidth);
  EXPECT_EQ(current_client_opts.enforce_topology_constraints,
            old_options.enforce_topology_constraints);
  EXPECT_EQ(current_client_opts.low_power, old_options.low_power);
  EXPECT_EQ(current_client_opts.is_out_of_band_connection,
            old_options.is_out_of_band_connection);
  EXPECT_EQ(current_client_opts.fast_advertisement_service_uuid,
            old_options.fast_advertisement_service_uuid);
  UpdateDiscoveryOptions(&client, &pcp_handler, new_options,
                         {Status::kSuccess});
  EXPECT_TRUE(client.IsDiscovering());
  current_client_opts = client.GetDiscoveryOptions();
  // check new option parameters
  EXPECT_EQ(current_client_opts.auto_upgrade_bandwidth,
            new_options.auto_upgrade_bandwidth);
  EXPECT_EQ(current_client_opts.enforce_topology_constraints,
            new_options.enforce_topology_constraints);
  EXPECT_EQ(current_client_opts.low_power, new_options.low_power);
  EXPECT_EQ(current_client_opts.is_out_of_band_connection,
            new_options.is_out_of_band_connection);
  EXPECT_EQ(current_client_opts.fast_advertisement_service_uuid,
            new_options.fast_advertisement_service_uuid);
  env_.Stop();
}

TEST_F(BasePcpHandlerTest, TestUpdateDiscoveryOptionsFailsWithBadStatus) {
  env_.Start();
  DiscoveryOptions old_options{
      {},
      true,   // auto_upgrade_bandwidth
      true,   // enforce_topology_constraints
      false,  // is_out_of_band_connection,
      "",     // fast_advertisement_service_uuid
      false,  // low_power
  };
  DiscoveryOptions new_options{
      {},
      true,   // auto_upgrade_bandwidth
      true,   // enforce_topology_constraints
      false,  // is_out_of_band_connection,
      "",     // fast_advertisement_service_uuid
      true,   // low_power
  };
  ClientProxy client;
  Mediums m;
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  BwuManager bwu(m, em, ecm, {}, {});
  MockPcpHandler pcp_handler(&m, &em, &ecm, &bwu);
  StartDiscoveryWithOptions(&client, &pcp_handler, old_options);
  EXPECT_TRUE(client.IsDiscovering());
  auto current_client_opts = client.GetDiscoveryOptions();
  // check custom option parameters
  EXPECT_EQ(current_client_opts.auto_upgrade_bandwidth,
            old_options.auto_upgrade_bandwidth);
  EXPECT_EQ(current_client_opts.enforce_topology_constraints,
            old_options.enforce_topology_constraints);
  EXPECT_EQ(current_client_opts.low_power, old_options.low_power);
  EXPECT_EQ(current_client_opts.is_out_of_band_connection,
            old_options.is_out_of_band_connection);
  EXPECT_EQ(current_client_opts.fast_advertisement_service_uuid,
            old_options.fast_advertisement_service_uuid);
  UpdateDiscoveryOptions(&client, &pcp_handler, new_options,
                         {Status::kBleError});
  EXPECT_TRUE(client.IsDiscovering());
  current_client_opts = client.GetDiscoveryOptions();
  // check new option parameters
  EXPECT_EQ(current_client_opts.auto_upgrade_bandwidth,
            old_options.auto_upgrade_bandwidth);
  EXPECT_EQ(current_client_opts.enforce_topology_constraints,
            old_options.enforce_topology_constraints);
  EXPECT_EQ(current_client_opts.low_power, old_options.low_power);
  EXPECT_EQ(current_client_opts.is_out_of_band_connection,
            old_options.is_out_of_band_connection);
  EXPECT_EQ(current_client_opts.fast_advertisement_service_uuid,
            old_options.fast_advertisement_service_uuid);
  env_.Stop();
}

}  // namespace
}  // namespace connections
}  // namespace nearby
