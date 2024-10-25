// Copyright 2022-2023 Google LLC
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

#include "sharing/nearby_connections_manager_impl.h"

#include <stdint.h>

#include <cstdio>
#include <filesystem>  // NOLINT(build/c++17)
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "internal/flags/nearby_flags.h"
#include "internal/test/fake_clock.h"
#include "internal/test/fake_device_info.h"
#include "internal/test/fake_task_runner.h"
#include "sharing/common/nearby_share_enums.h"
#include "sharing/constants.h"
#include "sharing/fake_nearby_connections_service.h"
#include "sharing/flags/generated/nearby_sharing_feature_flags.h"
#include "sharing/internal/public/connectivity_manager.h"
#include "sharing/internal/test/fake_connectivity_manager.h"
#include "sharing/internal/test/fake_context.h"
#include "sharing/nearby_connection.h"
#include "sharing/nearby_connections_manager.h"
#include "sharing/nearby_connections_service.h"
#include "sharing/nearby_connections_types.h"
#include "sharing/proto/enums.pb.h"
#include "sharing/transfer_manager.h"

namespace nearby {
namespace sharing {
namespace {

using ::nearby::sharing::proto::DataUsage;
using ::testing::ElementsAre;
using ::testing::FieldsAre;
using ::testing::UnorderedElementsAre;

constexpr char kServiceId[] = "NearbySharing";
constexpr Strategy kStrategy = Strategy::kP2pPointToPoint;
constexpr char kEndpointId[] = "endpoint_id";
constexpr char kRemoteEndpointId[] = "remote_endpoint_id";
constexpr char kAdvertisingServiceUuid[] =
    "0000fef3-0000-1000-8000-00805f9b34fb";

// The byte array is just used to set the endpoint information. In the test
// codes, no any special meanings.
constexpr char kEndpointInfo[] = {0x0d, 0x07, 0x07, 0x07, 0x07};
constexpr char kRemoteEndpointInfo[] = {0x0d, 0x07, 0x06, 0x08, 0x09};
constexpr char kAuthenticationToken[] = "authentication_token";
constexpr char kRawAuthenticationToken[] = {0x00, 0x05, 0x04, 0x03, 0x02};
constexpr char kBytePayload[] = {0x08, 0x09, 0x06, 0x04, 0x0f};
constexpr char kBytePayload2[] = {0x0a, 0x0b, 0x0c, 0x0d, 0x0e};
constexpr int64_t kPayloadId = 689777;
constexpr int64_t kPayloadId2 = 777689;
constexpr int64_t kPayloadId3 = 986777;
constexpr uint64_t kTotalSize = 5201314;
constexpr uint64_t kBytesTransferred = 721831;
constexpr uint8_t kPayload[] = {0x0f, 0x0a, 0x0c, 0x0e};
constexpr uint8_t kBluetoothMacAddress[] = {0x00, 0x00, 0xe6, 0x88, 0x64, 0x13};
constexpr char kInvalidBluetoothMacAddress[] = {0x07, 0x07, 0x07};
constexpr absl::Duration kSynchronizationTimeOut = absl::Milliseconds(200);

void InitializeTemporaryFile(std::filesystem::path& file) {
  std::FILE* output_fp = std::fopen(file.string().c_str(), "wb+");
  ASSERT_NE(output_fp, nullptr);
  EXPECT_EQ(std::fwrite(kPayload, 1, sizeof(kPayload), output_fp),
            sizeof(kPayload));
  std::fclose(output_fp);
}

}  // namespace

namespace NearbyConnectionsManagerUnitTests {

class MockDiscoveryListener
    : public NearbyConnectionsManager::DiscoveryListener {
 public:
  MOCK_METHOD(void, OnEndpointDiscovered,
              (absl::string_view endpoint_id,
               absl::Span<const uint8_t> endpoint_info),
              (override));
  MOCK_METHOD(void, OnEndpointLost, (absl::string_view endpoint_id),
              (override));
};

class MockIncomingConnectionListener
    : public NearbyConnectionsManager::IncomingConnectionListener {
 public:
  MOCK_METHOD(void, OnIncomingConnection,
              (absl::string_view endpoint_id,
               absl::Span<const uint8_t> endpoint_info,
               NearbyConnection* connection),
              (override));
};

class MockPayloadStatusListener
    : public NearbyConnectionsManager::PayloadStatusListener {
 public:
  MOCK_METHOD(void, OnStatusUpdate,
              (std::unique_ptr<PayloadTransferUpdate> update,
               std::optional<Medium> upgraded_medium),
              (override));
};

class NearbyConnectionsManagerImplTest : public testing::Test {
 public:
  void SetUp() override {
    NearbyFlags::GetInstance().OverrideBoolFlagValue(
        config_package_nearby::nearby_sharing_feature::kEnableMediumWebRtc,
        false);
    NearbyFlags::GetInstance().OverrideBoolFlagValue(
        config_package_nearby::nearby_sharing_feature::kEnableMediumWifiLan,
        true);
    auto nearby_connections_service =
        std::make_unique<testing::NiceMock<FakeNearbyConnectionsService>>();
    SetConnectionType(ConnectivityManager::ConnectionType::kWifi);
    nearby_connections_ = nearby_connections_service.get();

    nearby_connections_manager_ =
        std::make_unique<NearbyConnectionsManagerImpl>(
            &fake_task_runner_, &fake_context_, fake_connectivity_manager_,
            fake_device_info_, std::move(nearby_connections_service));
  }

  void TearDown() override {
    NearbyFlags::GetInstance().ResetOverridedValues();
    fake_task_runner_.SyncWithTimeout(absl::Seconds(1));
  }

  void SetConnectionType(ConnectivityManager::ConnectionType connection_type) {
    fake_connectivity_manager_.SetConnectionType(connection_type);
  }

  void Fastforward(absl::Duration duration) {
    fake_context_.fake_clock()->FastForward(duration);
  }

  void Sync() {
    EXPECT_TRUE(fake_task_runner_.SyncWithTimeout(absl::Seconds(1)));
  }

 protected:
  void StartDiscovery(
      NearbyConnectionsService::DiscoveryListener& listener_remote,
      testing::NiceMock<MockDiscoveryListener>& discovery_listener) {
    StartDiscovery(listener_remote, default_data_usage_, discovery_listener);
  }

  void StartDiscovery(
      NearbyConnectionsService::DiscoveryListener& listener_remote,
      DataUsage data_usage,
      testing::NiceMock<MockDiscoveryListener>& discovery_listener) {
    EXPECT_CALL(*nearby_connections_, StartDiscovery)
        .WillOnce([&listener_remote, this](
                      absl::string_view service_id, DiscoveryOptions options,
                      NearbyConnectionsService::DiscoveryListener listener,
                      std::function<void(Status status)> callback) {
          EXPECT_EQ(service_id, kServiceId);
          EXPECT_EQ(options.strategy, kStrategy);
          EXPECT_TRUE(options.allowed_mediums.bluetooth);
          EXPECT_TRUE(options.allowed_mediums.ble);
          EXPECT_EQ(options.allowed_mediums.web_rtc, should_use_web_rtc_);
          EXPECT_EQ(options.allowed_mediums.wifi_lan, should_use_wifilan_);
          EXPECT_EQ((*options.fast_advertisement_service_uuid).uuid,
                    kAdvertisingServiceUuid);

          listener_remote = std::move(listener);
          std::move(callback)(Status::kSuccess);
        });

    absl::Notification notification;
    NearbyConnectionsManager::ConnectionsCallback callback =
        [&notification](Status status) {
          EXPECT_EQ(status, Status::kSuccess);
          notification.Notify();
        };

    nearby_connections_manager_->StartDiscovery(&discovery_listener, data_usage,
                                                std::move(callback));

    EXPECT_TRUE(
        notification.WaitForNotificationWithTimeout(kSynchronizationTimeOut));
  }

  void StartAdvertising(
      NearbyConnectionsService::ConnectionListener& listener_remote,
      testing::NiceMock<MockIncomingConnectionListener>&
          incoming_connection_listener) {
    const std::vector<uint8_t> local_endpoint_info(std::begin(kEndpointInfo),
                                                   std::end(kEndpointInfo));
    EXPECT_CALL(*nearby_connections_, StartAdvertising)
        .WillOnce([&](absl::string_view service_id,
                      const std::vector<uint8_t>& endpoint_info,
                      AdvertisingOptions options,
                      NearbyConnectionsService::ConnectionListener listener,
                      std::function<void(Status status)> callback) {
          EXPECT_EQ(service_id, kServiceId);
          EXPECT_EQ(endpoint_info, local_endpoint_info);
          EXPECT_EQ(options.strategy, kStrategy);
          EXPECT_TRUE(options.enforce_topology_constraints);

          listener_remote = std::move(listener);
          std::move(callback)(Status::kSuccess);
        });

    absl::Notification notification;
    NearbyConnectionsManager::ConnectionsCallback callback =
        [&](Status status) {
          EXPECT_EQ(status, Status::kSuccess);
          notification.Notify();
        };
    nearby_connections_manager_->StartAdvertising(
        local_endpoint_info, &incoming_connection_listener,
        PowerLevel::kHighPower, DataUsage::ONLINE_DATA_USAGE, false,
        std::move(callback));
    EXPECT_TRUE(
        notification.WaitForNotificationWithTimeout(kSynchronizationTimeOut));
  }

  enum class ConnectionResponse { kAccepted, kRejected, kDisconnected };

  NearbyConnection* Connect(
      NearbyConnectionsService::ConnectionListener& connection_listener_remote,
      NearbyConnectionsService::PayloadListener& payload_listener_remote,
      ConnectionResponse connection_response) {
    const std::vector<uint8_t> local_endpoint_info(std::begin(kEndpointInfo),
                                                   std::end(kEndpointInfo));
    const std::vector<uint8_t> remote_endpoint_info(
        std::begin(kRemoteEndpointInfo), std::end(kRemoteEndpointInfo));
    const std::vector<uint8_t> raw_authentication_token(
        std::begin(kRawAuthenticationToken), std::end(kRawAuthenticationToken));

    absl::Notification request_connection_notification;
    EXPECT_CALL(*nearby_connections_, RequestConnection)
        .WillOnce([&](absl::string_view service_id,
                      const std::vector<uint8_t>& endpoint_info,
                      absl::string_view endpoint_id,
                      ConnectionOptions connection_options,
                      NearbyConnectionsService::ConnectionListener listener,
                      std::function<void(Status status)> callback) {
          EXPECT_EQ(kServiceId, service_id);
          EXPECT_EQ(local_endpoint_info, endpoint_info);
          EXPECT_EQ(kRemoteEndpointId, endpoint_id);

          connection_listener_remote = std::move(listener);
          std::move(callback)(Status::kSuccess);
          request_connection_notification.Notify();
        });

    absl::Notification notification;
    NearbyConnection* nearby_connection;
    nearby_connections_manager_->Connect(
        local_endpoint_info, kRemoteEndpointId,
        /*bluetooth_mac_address=*/std::nullopt, DataUsage::OFFLINE_DATA_USAGE,
        TransportType::kHighQuality,
        [&](NearbyConnection* connection, Status status) {
          nearby_connection = connection;
          notification.Notify();
        });

    EXPECT_TRUE(request_connection_notification.WaitForNotificationWithTimeout(
        kSynchronizationTimeOut));

    EXPECT_CALL(*nearby_connections_, AcceptConnection)
        .WillOnce([&](absl::string_view service_id,
                      absl::string_view endpoint_id,
                      NearbyConnectionsService::PayloadListener listener,
                      std::function<void(Status status)> callback) {
          EXPECT_EQ(kServiceId, service_id);
          EXPECT_EQ(kRemoteEndpointId, endpoint_id);

          payload_listener_remote = std::move(listener);
          std::move(callback)(Status::kSuccess);
        });

    ConnectionInfo connection_info;
    connection_info.authentication_token = kAuthenticationToken;
    connection_info.raw_authentication_token = raw_authentication_token;
    connection_info.endpoint_info = remote_endpoint_info;
    connection_info.is_incoming_connection = false;

    connection_listener_remote.initiated_cb(kRemoteEndpointId, connection_info);

    switch (connection_response) {
      case ConnectionResponse::kAccepted:
        connection_listener_remote.accepted_cb(kRemoteEndpointId);
        break;
      case ConnectionResponse::kRejected:
        connection_listener_remote.rejected_cb(kRemoteEndpointId,
                                               Status::kConnectionRejected);
        break;
      case ConnectionResponse::kDisconnected:
        connection_listener_remote.disconnected_cb(kRemoteEndpointId);
        break;
    }
    EXPECT_TRUE(
        notification.WaitForNotificationWithTimeout(kSynchronizationTimeOut));

    return nearby_connection;
  }

  NearbyConnection* OnIncomingConnection(
      NearbyConnectionsService::ConnectionListener& connection_listener_remote,
      testing::NiceMock<MockIncomingConnectionListener>&
          incoming_connection_listener,
      NearbyConnectionsService::PayloadListener& payload_listener_remote) {
    absl::Notification accept_notification;

    EXPECT_CALL(*nearby_connections_, AcceptConnection)
        .WillOnce([&](absl::string_view service_id,
                      absl::string_view endpoint_id,
                      NearbyConnectionsService::PayloadListener listener,
                      std::function<void(Status status)> callback) {
          EXPECT_EQ(service_id, kServiceId);
          EXPECT_EQ(endpoint_id, kRemoteEndpointId);
          payload_listener_remote = std::move(listener);
          std::move(callback)(Status::kSuccess);
          accept_notification.Notify();
        });

    const std::vector<uint8_t> remote_endpoint_info(
        std::begin(kRemoteEndpointInfo), std::end(kRemoteEndpointInfo));
    const std::vector<uint8_t> raw_authentication_token(
        std::begin(kRawAuthenticationToken), std::end(kRawAuthenticationToken));

    ConnectionInfo connection_info;
    connection_info.authentication_token = kAuthenticationToken;
    connection_info.raw_authentication_token = raw_authentication_token;
    connection_info.endpoint_info = remote_endpoint_info;
    connection_info.is_incoming_connection = true;

    connection_listener_remote.initiated_cb(kRemoteEndpointId, connection_info);
    EXPECT_TRUE(accept_notification.WaitForNotificationWithTimeout(
        kSynchronizationTimeOut));

    NearbyConnection* nearby_connection;
    absl::Notification incoming_connection_notification;
    EXPECT_CALL(incoming_connection_listener,
                OnIncomingConnection(testing::_, testing::_, testing::_))
        .WillOnce([&](absl::string_view endpoint_id,
                      absl::Span<const uint8_t> endpoint_info,
                      NearbyConnection* connection) {
          nearby_connection = connection;
          incoming_connection_notification.Notify();
        });

    connection_listener_remote.accepted_cb(kRemoteEndpointId);
    EXPECT_TRUE(incoming_connection_notification.WaitForNotificationWithTimeout(
        kSynchronizationTimeOut));

    EXPECT_EQ(nearby_connections_manager_->GetRawAuthenticationToken(
                  kRemoteEndpointId),
              raw_authentication_token);

    return nearby_connection;
  }

  void SendPayload(int64_t payload_id,
                   std::shared_ptr<testing::NiceMock<MockPayloadStatusListener>>
                       payload_listener) {
    const std::vector<uint8_t> expected_payload(std::begin(kPayload),
                                                std::end(kPayload));

    std::filesystem::path file(std::filesystem::temp_directory_path() /
                               "file.jpg");
    InitializeTemporaryFile(file);

    absl::Notification notification;
    EXPECT_CALL(*nearby_connections_, SendPayload)
        .WillOnce([&](absl::string_view service_id,
                      absl::Span<const std::string> endpoint_ids,
                      std::unique_ptr<Payload> payload,
                      std::function<void(Status status)> callback) {
          EXPECT_EQ(service_id, kServiceId);
          EXPECT_THAT(endpoint_ids, ElementsAre(kRemoteEndpointId));
          ASSERT_NE(payload, nullptr);
          EXPECT_EQ(payload_id, payload->id);

          FilePayload file_payload = std::move(payload->content.file_payload);
          std::vector<uint8_t> payload_bytes(file_payload.size);
          std::FILE* payload_fp =
              std::fopen(file_payload.file.path.string().c_str(), "rb");
          ASSERT_NE(payload_fp, nullptr);
          EXPECT_EQ(std::fread(payload_bytes.data(), 1, file_payload.size,
                               payload_fp),
                    file_payload.size);
          EXPECT_EQ(expected_payload, payload_bytes);
          std::fclose(payload_fp);

          std::move(callback)(Status::kSuccess);
          notification.Notify();
        });

    // Manually setup payload id, because the tested id is not generated from
    // file name.
    auto payload = std::make_unique<Payload>(InputFile(file));
    payload->id = payload_id;

    nearby_connections_manager_->Send(kRemoteEndpointId, std::move(payload),
                                      payload_listener->GetWeakPtr());
    // Move forward timer to trigger payload sending.
    Fastforward(TransferManager::kMediumUpgradeTimeout);

    EXPECT_TRUE(
        notification.WaitForNotificationWithTimeout(kSynchronizationTimeOut));
  }

  nearby::FakeContext fake_context_;
  FakeTaskRunner fake_task_runner_{fake_context_.fake_clock(), 1};
  nearby::FakeDeviceInfo fake_device_info_;
  nearby::FakeConnectivityManager fake_connectivity_manager_;
  bool should_use_web_rtc_ = false;
  bool should_use_wifilan_ = true;
  DataUsage default_data_usage_ = DataUsage::WIFI_ONLY_DATA_USAGE;

  testing::NiceMock<FakeNearbyConnectionsService>* nearby_connections_;

  std::unique_ptr<NearbyConnectionsManagerImpl> nearby_connections_manager_;
};

TEST_F(NearbyConnectionsManagerImplTest, DiscoveryFlow) {
  const std::vector<uint8_t> endpoint_info(std::begin(kEndpointInfo),
                                           std::end(kEndpointInfo));

  // StartDiscovery will succeed.
  NearbyConnectionsService::DiscoveryListener listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(listener_remote, discovery_listener);

  // Invoking OnEndpointFound over remote will invoke OnEndpointDiscovered.
  absl::Notification discovered_notification;
  EXPECT_CALL(discovery_listener,
              OnEndpointDiscovered(testing::Eq(kEndpointId),
                                   testing::Eq(endpoint_info)))
      .WillOnce(
          [&discovered_notification]() { discovered_notification.Notify(); });
  listener_remote.endpoint_found_cb(
      kEndpointId, DiscoveredEndpointInfo(endpoint_info, kServiceId));
  EXPECT_TRUE(discovered_notification.WaitForNotificationWithTimeout(
      kSynchronizationTimeOut));

  // Invoking OnEndpointFound over remote on same endpointId will do nothing.
  EXPECT_CALL(discovery_listener, OnEndpointDiscovered(testing::_, testing::_))
      .Times(0);
  listener_remote.endpoint_found_cb(
      kEndpointId, DiscoveredEndpointInfo(endpoint_info, kServiceId));

  // Invoking OnEndpointLost over remote will invoke OnEndpointLost.
  absl::Notification lost_notification;
  EXPECT_CALL(discovery_listener, OnEndpointLost(testing::Eq(kEndpointId)))
      .WillOnce([&lost_notification]() { lost_notification.Notify(); });
  listener_remote.endpoint_lost_cb(kEndpointId);
  EXPECT_TRUE(lost_notification.WaitForNotificationWithTimeout(
      kSynchronizationTimeOut));

  // Invoking OnEndpointLost over remote on same endpointId will do nothing.
  EXPECT_CALL(discovery_listener, OnEndpointLost(testing::_)).Times(0);
  listener_remote.endpoint_lost_cb(kEndpointId);

  // After OnEndpointLost the same endpointId can be discovered again.
  absl::Notification discovered_notification2;
  EXPECT_CALL(discovery_listener,
              OnEndpointDiscovered(testing::Eq(kEndpointId),
                                   testing::Eq(endpoint_info)))
      .WillOnce(
          [&discovered_notification2]() { discovered_notification2.Notify(); });
  listener_remote.endpoint_found_cb(
      kEndpointId, DiscoveredEndpointInfo(endpoint_info, kServiceId));
  EXPECT_TRUE(discovered_notification2.WaitForNotificationWithTimeout(
      kSynchronizationTimeOut));

  // Stop discovery will call through nearby sharing service.
  absl::Notification stop_discovery_notification;
  EXPECT_CALL(*nearby_connections_, StopDiscovery)
      .WillOnce([&stop_discovery_notification](
                    absl::string_view service_id,
                    std::function<void(Status status)> callback) {
        EXPECT_EQ(service_id, kServiceId);
        std::move(callback)(Status::kSuccess);
        stop_discovery_notification.Notify();
      });
  nearby_connections_manager_->StopDiscovery();
  EXPECT_TRUE(stop_discovery_notification.WaitForNotificationWithTimeout(
      kSynchronizationTimeOut));

  // StartDiscovery again will succeed.
  StartDiscovery(listener_remote, discovery_listener);

  // Same endpointId can be discovered again.
  absl::Notification discovered_notification_3;
  EXPECT_CALL(discovery_listener,
              OnEndpointDiscovered(testing::Eq(kEndpointId),
                                   testing::Eq(endpoint_info)))
      .WillOnce([&discovered_notification_3]() {
        discovered_notification_3.Notify();
      });
  listener_remote.endpoint_found_cb(
      kEndpointId, DiscoveredEndpointInfo(endpoint_info, kServiceId));
  EXPECT_TRUE(discovered_notification_3.WaitForNotificationWithTimeout(
      kSynchronizationTimeOut));
}

TEST_F(NearbyConnectionsManagerImplTest, DisableWifiHotspot) {
  SetConnectionType(ConnectivityManager::ConnectionType::kWifi);

  MediumSelection expected_mediums(/*bluetooth=*/true,
                                   /*ble=*/false,
                                   /*web_rtc=*/should_use_web_rtc_,
                                   /*wifi_lan=*/should_use_wifilan_,
                                   /*wifi_hotspot*/ true);

  // StartDiscovery will succeed.
  NearbyConnectionsService::DiscoveryListener discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, DataUsage::WIFI_ONLY_DATA_USAGE,
                 discovery_listener);

  absl::Notification notification;
  const std::vector<uint8_t> local_endpoint_info(std::begin(kEndpointInfo),
                                                 std::end(kEndpointInfo));
  EXPECT_CALL(*nearby_connections_, RequestConnection)
      .WillOnce([&](absl::string_view service_id,
                    const std::vector<uint8_t>& endpoint_info,
                    absl::string_view endpoint_id, ConnectionOptions options,
                    NearbyConnectionsService::ConnectionListener listener,
                    std::function<void(Status status)> callback) {
        EXPECT_EQ(service_id, kServiceId);
        EXPECT_EQ(endpoint_info, local_endpoint_info);
        EXPECT_EQ(endpoint_id, kRemoteEndpointId);
        EXPECT_EQ(options.allowed_mediums.wifi_hotspot, true);
        EXPECT_TRUE(options.non_disruptive_hotspot_mode);
        std::move(callback)(Status::kSuccess);
        notification.Notify();
      });

  NearbyConnectionsManager::NearbyConnectionCallback connections_callback;

  nearby_connections_manager_->Connect(
      local_endpoint_info, kRemoteEndpointId,
      /*bluetooth_mac_address=*/std::nullopt, DataUsage::WIFI_ONLY_DATA_USAGE,
      TransportType::kHighQualityNonDisruptive, connections_callback);

  EXPECT_TRUE(
      notification.WaitForNotificationWithTimeout(kSynchronizationTimeOut));
}

/******************************************************************************/
// Begin: NearbyConnectionsManagerImplTestConnectionMediums
/******************************************************************************/
using ConnectionMediumsTestParam =
    std::tuple<DataUsage, ConnectivityManager::ConnectionType, bool, bool>;
class NearbyConnectionsManagerImplTestConnectionMediums
    : public NearbyConnectionsManagerImplTest,
      public testing::WithParamInterface<ConnectionMediumsTestParam> {};

TEST_P(NearbyConnectionsManagerImplTestConnectionMediums,
       RequestConnection_MediumSelection) {
  const ConnectionMediumsTestParam& param = GetParam();
  DataUsage data_usage = std::get<0>(param);
  ConnectivityManager::ConnectionType connection_type = std::get<1>(param);
  bool is_webrtc_enabled = std::get<2>(GetParam());
  bool is_wifilan_enabled = std::get<3>(GetParam());

  if (is_webrtc_enabled) {
    NearbyFlags::GetInstance().OverrideBoolFlagValue(
        config_package_nearby::nearby_sharing_feature::kEnableMediumWebRtc,
        true);
  } else {
    NearbyFlags::GetInstance().OverrideBoolFlagValue(
        config_package_nearby::nearby_sharing_feature::kEnableMediumWebRtc,
        false);
  }
  if (is_wifilan_enabled) {
    NearbyFlags::GetInstance().OverrideBoolFlagValue(
        config_package_nearby::nearby_sharing_feature::kEnableMediumWifiLan,
        true);
  } else {
    NearbyFlags::GetInstance().OverrideBoolFlagValue(
        config_package_nearby::nearby_sharing_feature::kEnableMediumWifiLan,
        false);
  }

  SetConnectionType(connection_type);
  bool should_use_internet =
      data_usage != DataUsage::OFFLINE_DATA_USAGE &&
      connection_type != ConnectivityManager::ConnectionType::kNone &&
      !(data_usage == DataUsage::WIFI_ONLY_DATA_USAGE &&
        connection_type != ConnectivityManager::ConnectionType::kWifi);
  bool is_connection_wifi_or_ethernet =
      connection_type == ConnectivityManager::ConnectionType::kWifi ||
      connection_type == ConnectivityManager::ConnectionType::kEthernet;
  should_use_web_rtc_ = is_webrtc_enabled && should_use_internet;
  should_use_wifilan_ = is_wifilan_enabled && is_connection_wifi_or_ethernet;

  MediumSelection expected_mediums(/*bluetooth=*/true,
                                   /*ble=*/false,
                                   /*web_rtc=*/should_use_web_rtc_,
                                   /*wifi_lan=*/should_use_wifilan_,
                                   /*wifi_hotspot*/ true);

  // StartDiscovery will succeed.
  NearbyConnectionsService::DiscoveryListener discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, data_usage, discovery_listener);

  absl::Notification notification;
  const std::vector<uint8_t> local_endpoint_info(std::begin(kEndpointInfo),
                                                 std::end(kEndpointInfo));
  EXPECT_CALL(*nearby_connections_, RequestConnection)
      .WillOnce([&](absl::string_view service_id,
                    const std::vector<uint8_t>& endpoint_info,
                    absl::string_view endpoint_id, ConnectionOptions options,
                    NearbyConnectionsService::ConnectionListener listener,
                    std::function<void(Status status)> callback) {
        EXPECT_EQ(service_id, kServiceId);
        EXPECT_EQ(endpoint_info, local_endpoint_info);
        EXPECT_EQ(endpoint_id, kRemoteEndpointId);
        EXPECT_EQ(options.allowed_mediums.ble, expected_mediums.ble);
        EXPECT_EQ(options.allowed_mediums.bluetooth,
                  expected_mediums.bluetooth);
        EXPECT_EQ(options.allowed_mediums.web_rtc, expected_mediums.web_rtc);
        EXPECT_EQ(options.allowed_mediums.wifi_lan, expected_mediums.wifi_lan);
        EXPECT_EQ(options.allowed_mediums.wifi_hotspot,
                  expected_mediums.wifi_hotspot);
        EXPECT_FALSE(options.non_disruptive_hotspot_mode);
        std::move(callback)(Status::kSuccess);
        notification.Notify();
      });

  NearbyConnectionsManager::NearbyConnectionCallback connections_callback;

  nearby_connections_manager_->Connect(local_endpoint_info, kRemoteEndpointId,
                                       /*bluetooth_mac_address=*/std::nullopt,
                                       data_usage, TransportType::kHighQuality,
                                       connections_callback);

  EXPECT_TRUE(
      notification.WaitForNotificationWithTimeout(kSynchronizationTimeOut));
}

INSTANTIATE_TEST_SUITE_P(
    NearbyConnectionsManagerImplTestConnectionMediums,
    NearbyConnectionsManagerImplTestConnectionMediums,
    testing::Combine(testing::Values(DataUsage::WIFI_ONLY_DATA_USAGE,
                                     DataUsage::OFFLINE_DATA_USAGE,
                                     DataUsage::ONLINE_DATA_USAGE),
                     testing::Values(ConnectivityManager::ConnectionType::kNone,
                                     ConnectivityManager::ConnectionType::kWifi,
                                     ConnectivityManager::ConnectionType::k3G),
                     testing::Bool(), testing::Bool()));
/******************************************************************************/
// End: NearbyConnectionsManagerImplTestConnectionMediums
/******************************************************************************/

/******************************************************************************/
// Begin: NearbyConnectionsManagerImplTestConnectionBluetoothMacAddress
/******************************************************************************/
typedef struct {
  std::optional<std::vector<uint8_t>> bluetooth_mac_address;
  std::optional<std::vector<uint8_t>> expected_bluetooth_mac_address;
} ConnectionBluetoothMacAddressTestData;

const std::vector<ConnectionBluetoothMacAddressTestData>&
GetConnectionBluetoothMacAddressTestData() {
  static std::vector<ConnectionBluetoothMacAddressTestData>* data =
      new std::vector<ConnectionBluetoothMacAddressTestData>{
          {std::make_optional(
               std::vector<uint8_t>(std::begin(kBluetoothMacAddress),
                                    std::end(kBluetoothMacAddress))),
           std::make_optional(
               std::vector<uint8_t>(std::begin(kBluetoothMacAddress),
                                    std::end(kBluetoothMacAddress)))},
          {std::make_optional(
               std::vector<uint8_t>(std::begin(kInvalidBluetoothMacAddress),
                                    std::end(kInvalidBluetoothMacAddress))),
           std::nullopt},
          {std::nullopt, std::nullopt}};
  return *data;
}

class NearbyConnectionsManagerImplTestConnectionBluetoothMacAddress
    : public NearbyConnectionsManagerImplTest,
      public testing::WithParamInterface<
          ConnectionBluetoothMacAddressTestData> {};

TEST_P(NearbyConnectionsManagerImplTestConnectionBluetoothMacAddress,
       RequestConnection_BluetoothMacAddress) {
  // StartDiscovery will succeed.
  NearbyConnectionsService::DiscoveryListener discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  absl::Notification notification;
  const std::vector<uint8_t> local_endpoint_info(std::begin(kEndpointInfo),
                                                 std::end(kEndpointInfo));
  EXPECT_CALL(*nearby_connections_, RequestConnection)
      .WillOnce([&](absl::string_view service_id,
                    const std::vector<uint8_t>& endpoint_info,
                    absl::string_view endpoint_id, ConnectionOptions options,
                    NearbyConnectionsService::ConnectionListener listener,
                    std::function<void(Status status)> callback) {
        EXPECT_EQ(service_id, kServiceId);
        EXPECT_EQ(endpoint_info, local_endpoint_info);
        EXPECT_EQ(endpoint_id, kRemoteEndpointId);
        EXPECT_EQ(GetParam().expected_bluetooth_mac_address,
                  options.remote_bluetooth_mac_address);
        std::move(callback)(Status::kSuccess);
        notification.Notify();
      });

  NearbyConnectionsManager::NearbyConnectionCallback connections_callback;
  nearby_connections_manager_->Connect(
      local_endpoint_info, kRemoteEndpointId, GetParam().bluetooth_mac_address,
      DataUsage::OFFLINE_DATA_USAGE, TransportType::kHighQuality,
      connections_callback);

  EXPECT_TRUE(
      notification.WaitForNotificationWithTimeout(kSynchronizationTimeOut));
}

INSTANTIATE_TEST_SUITE_P(
    NearbyConnectionsManagerImplTestConnectionBluetoothMacAddress,
    NearbyConnectionsManagerImplTestConnectionBluetoothMacAddress,
    testing::ValuesIn(GetConnectionBluetoothMacAddressTestData()));
/******************************************************************************/
// End: NearbyConnectionsManagerImplTestConnectionBluetoothMacAddress
/******************************************************************************/

TEST_F(NearbyConnectionsManagerImplTest, ConnectRejected) {
  // StartDiscovery will succeed.
  NearbyConnectionsService::DiscoveryListener discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  // RequestConnection will succeed.
  NearbyConnectionsService::ConnectionListener connection_listener_remote;
  NearbyConnectionsService::PayloadListener payload_listener_remote;
  NearbyConnection* nearby_connection =
      Connect(connection_listener_remote, payload_listener_remote,
              ConnectionResponse::kRejected);
  EXPECT_FALSE(nearby_connection);
  EXPECT_FALSE(nearby_connections_manager_->GetRawAuthenticationToken(
      kRemoteEndpointId));
}

TEST_F(NearbyConnectionsManagerImplTest, ConnectDisconnected) {
  // StartDiscovery will succeed.
  NearbyConnectionsService::DiscoveryListener discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  // RequestConnection will succeed.
  NearbyConnectionsService::ConnectionListener connection_listener_remote;
  NearbyConnectionsService::PayloadListener payload_listener_remote;
  NearbyConnection* nearby_connection =
      Connect(connection_listener_remote, payload_listener_remote,
              ConnectionResponse::kDisconnected);
  EXPECT_TRUE(fake_task_runner_.SyncWithTimeout(absl::Seconds(1)));
  EXPECT_FALSE(nearby_connection);
  EXPECT_FALSE(nearby_connections_manager_->GetRawAuthenticationToken(
      kRemoteEndpointId));
}

TEST_F(NearbyConnectionsManagerImplTest, ConnectAccepted) {
  const std::vector<uint8_t> raw_authentication_token(
      std::begin(kRawAuthenticationToken), std::end(kRawAuthenticationToken));

  // StartDiscovery will succeed.
  NearbyConnectionsService::DiscoveryListener discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  // RequestConnection will succeed.
  NearbyConnectionsService::ConnectionListener connection_listener_remote;
  NearbyConnectionsService::PayloadListener payload_listener_remote;
  NearbyConnection* nearby_connection =
      Connect(connection_listener_remote, payload_listener_remote,
              ConnectionResponse::kAccepted);
  EXPECT_TRUE(nearby_connection);
  EXPECT_EQ(
      nearby_connections_manager_->GetRawAuthenticationToken(kRemoteEndpointId),
      raw_authentication_token);
}

TEST_F(NearbyConnectionsManagerImplTest, ConnectReadBeforeAppend) {
  const std::vector<uint8_t> byte_payload(std::begin(kBytePayload),
                                          std::end(kBytePayload));

  // StartDiscovery will succeed.
  NearbyConnectionsService::DiscoveryListener discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  // RequestConnection will succeed.
  NearbyConnectionsService::ConnectionListener connection_listener_remote;
  NearbyConnectionsService::PayloadListener payload_listener_remote;
  NearbyConnection* nearby_connection =
      Connect(connection_listener_remote, payload_listener_remote,
              ConnectionResponse::kAccepted);
  ASSERT_TRUE(nearby_connection);

  // Read before message is appended should also succeed.
  absl::Notification notification;
  nearby_connection->Read([&](std::optional<std::vector<uint8_t>> bytes) {
    EXPECT_EQ(bytes, byte_payload);
    notification.Notify();
  });
  Sync();
  payload_listener_remote.payload_cb(kRemoteEndpointId,
                                     Payload(kPayloadId, byte_payload));
  payload_listener_remote.payload_progress_cb(
      kRemoteEndpointId,
      PayloadTransferUpdate(kPayloadId, PayloadStatus::kSuccess, kTotalSize,
                            /*bytes_transferred=*/kTotalSize));
  EXPECT_TRUE(
      notification.WaitForNotificationWithTimeout(kSynchronizationTimeOut));
}

TEST_F(NearbyConnectionsManagerImplTest, ConnectReadAfterAppend) {
  const std::vector<uint8_t> byte_payload(std::begin(kBytePayload),
                                          std::end(kBytePayload));
  const std::vector<uint8_t> byte_payload_2(std::begin(kBytePayload2),
                                            std::end(kBytePayload2));

  // StartDiscovery will succeed.
  NearbyConnectionsService::DiscoveryListener discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  // RequestConnection will succeed.
  NearbyConnectionsService::ConnectionListener connection_listener_remote;
  NearbyConnectionsService::PayloadListener payload_listener_remote;
  NearbyConnection* nearby_connection =
      Connect(connection_listener_remote, payload_listener_remote,
              ConnectionResponse::kAccepted);
  ASSERT_NE(nearby_connection, nullptr);

  // Read after message is appended should succeed.
  payload_listener_remote.payload_cb(kRemoteEndpointId,
                                     Payload(kPayloadId, byte_payload));
  payload_listener_remote.payload_progress_cb(
      kRemoteEndpointId,
      PayloadTransferUpdate(kPayloadId, PayloadStatus::kSuccess, kTotalSize,
                            /*bytes_transferred=*/kTotalSize));
  payload_listener_remote.payload_cb(kRemoteEndpointId,
                                     Payload(kPayloadId2, byte_payload_2));
  payload_listener_remote.payload_progress_cb(
      kRemoteEndpointId,
      PayloadTransferUpdate(kPayloadId2, PayloadStatus::kSuccess, kTotalSize,
                            /*bytes_transferred=*/kTotalSize));

  absl::Notification read_notification;
  nearby_connection->Read([&](std::optional<std::vector<uint8_t>> bytes) {
    ASSERT_TRUE(bytes.has_value());
    EXPECT_EQ(bytes, byte_payload);
    read_notification.Notify();
  });
  Sync();
  EXPECT_TRUE(read_notification.WaitForNotificationWithTimeout(
      kSynchronizationTimeOut));

  absl::Notification read_notification2;
  nearby_connection->Read([&](std::optional<std::vector<uint8_t>> bytes) {
    EXPECT_EQ(bytes, byte_payload_2);
    read_notification2.Notify();
  });
  Sync();
  EXPECT_TRUE(read_notification2.WaitForNotificationWithTimeout(
      kSynchronizationTimeOut));
}

TEST_F(NearbyConnectionsManagerImplTest, ConnectWrite) {
  const std::vector<uint8_t> byte_payload(std::begin(kBytePayload),
                                          std::end(kBytePayload));
  // StartDiscovery will succeed.
  NearbyConnectionsService::DiscoveryListener discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  // RequestConnection will succeed.
  NearbyConnectionsService::ConnectionListener connection_listener_remote;
  NearbyConnectionsService::PayloadListener payload_listener_remote;
  NearbyConnection* nearby_connection =
      Connect(connection_listener_remote, payload_listener_remote,
              ConnectionResponse::kAccepted);
  ASSERT_TRUE(nearby_connection);

  absl::Notification notification;
  EXPECT_CALL(*nearby_connections_, SendPayload)
      .WillOnce([&](absl::string_view service_id,
                    absl::Span<const std::string> endpoint_ids,
                    std::unique_ptr<Payload> payload,
                    std::function<void(Status status)> callback) {
        EXPECT_EQ(service_id, kServiceId);
        EXPECT_THAT(endpoint_ids, ElementsAre(kRemoteEndpointId));
        ASSERT_TRUE(payload);
        ASSERT_TRUE(payload->content.is_bytes());
        EXPECT_EQ(payload->content.bytes_payload.bytes, byte_payload);

        std::move(callback)(Status::kSuccess);
        notification.Notify();
      });

  nearby_connection->Write(byte_payload);
  Sync();
  EXPECT_TRUE(
      notification.WaitForNotificationWithTimeout(kSynchronizationTimeOut));
}

TEST_F(NearbyConnectionsManagerImplTest, ConnectClosed) {
  // StartDiscovery will succeed.
  NearbyConnectionsService::DiscoveryListener discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  // RequestConnection will succeed.
  NearbyConnectionsService::ConnectionListener connection_listener_remote;
  NearbyConnectionsService::PayloadListener payload_listener_remote;
  NearbyConnection* nearby_connection =
      Connect(connection_listener_remote, payload_listener_remote,
              ConnectionResponse::kAccepted);
  ASSERT_TRUE(nearby_connection);

  // Close should invoke disconnection callback and read callback.
  absl::Notification close_notification;
  nearby_connection->SetDisconnectionListener(
      [&]() { close_notification.Notify(); });
  Sync();
  absl::Notification read_notification;
  nearby_connection->Read([&](std::optional<std::vector<uint8_t>> bytes) {
    EXPECT_FALSE(bytes.has_value());
    read_notification.Notify();
  });
  Sync();

  absl::Notification disconnect_notification;
  EXPECT_CALL(*nearby_connections_, DisconnectFromEndpoint)
      .WillOnce([&](absl::string_view service_id, absl::string_view endpoint_id,
                    std::function<void(Status status)> callback) {
        EXPECT_EQ(service_id, kServiceId);
        EXPECT_EQ(endpoint_id, kRemoteEndpointId);
        std::move(callback)(Status::kSuccess);
        disconnect_notification.Notify();
      });

  nearby_connection->Close();
  Sync();

  EXPECT_TRUE(close_notification.WaitForNotificationWithTimeout(
      kSynchronizationTimeOut));
  EXPECT_TRUE(read_notification.WaitForNotificationWithTimeout(
      kSynchronizationTimeOut));
  EXPECT_TRUE(disconnect_notification.WaitForNotificationWithTimeout(
      kSynchronizationTimeOut));
  EXPECT_FALSE(nearby_connections_manager_->GetRawAuthenticationToken(
      kRemoteEndpointId));
}

TEST_F(NearbyConnectionsManagerImplTest, ConnectClosedByRemote) {
  // StartDiscovery will succeed.
  NearbyConnectionsService::DiscoveryListener discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  // RequestConnection will succeed.
  NearbyConnectionsService::ConnectionListener connection_listener_remote;
  NearbyConnectionsService::PayloadListener payload_listener_remote;
  NearbyConnection* nearby_connection =
      Connect(connection_listener_remote, payload_listener_remote,
              ConnectionResponse::kAccepted);
  ASSERT_TRUE(nearby_connection);

  // Remote closing should invoke disconnection callback and read callback.
  absl::Notification close_notification;
  nearby_connection->SetDisconnectionListener(
      [&]() { close_notification.Notify(); });
  Sync();
  absl::Notification read_notification;
  nearby_connection->Read([&](absl::optional<std::vector<uint8_t>> bytes) {
    EXPECT_FALSE(bytes);
    read_notification.Notify();
  });
  Sync();
  connection_listener_remote.disconnected_cb(kRemoteEndpointId);
  EXPECT_TRUE(close_notification.WaitForNotificationWithTimeout(
      kSynchronizationTimeOut));
  EXPECT_TRUE(read_notification.WaitForNotificationWithTimeout(
      kSynchronizationTimeOut));
  EXPECT_FALSE(nearby_connections_manager_->GetRawAuthenticationToken(
      kRemoteEndpointId));
}

TEST_F(NearbyConnectionsManagerImplTest, ConnectClosedByClient) {
  // StartDiscovery will succeed.
  NearbyConnectionsService::DiscoveryListener discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  // RequestConnection will succeed.
  NearbyConnectionsService::ConnectionListener connection_listener_remote;
  NearbyConnectionsService::PayloadListener payload_listener_remote;
  NearbyConnection* nearby_connection =
      Connect(connection_listener_remote, payload_listener_remote,
              ConnectionResponse::kAccepted);
  ASSERT_TRUE(nearby_connection);

  // Remote closing should invoke disconnection callback and read callback.
  absl::Notification close_notification;
  nearby_connection->SetDisconnectionListener(
      [&]() { close_notification.Notify(); });
  Sync();
  absl::Notification read_notification;
  nearby_connection->Read([&](absl::optional<std::vector<uint8_t>> bytes) {
    EXPECT_FALSE(bytes);
    read_notification.Notify();
  });
  Sync();

  absl::Notification disconnect_notification;
  EXPECT_CALL(*nearby_connections_, DisconnectFromEndpoint)
      .WillOnce([&](absl::string_view service_id, absl::string_view endpoint_id,
                    std::function<void(Status status)> callback) {
        EXPECT_EQ(service_id, kServiceId);
        EXPECT_EQ(endpoint_id, kRemoteEndpointId);
        std::move(callback)(Status::kSuccess);
        disconnect_notification.Notify();
      });
  nearby_connections_manager_->Disconnect(kRemoteEndpointId);
  Sync();
  EXPECT_TRUE(close_notification.WaitForNotificationWithTimeout(
      kSynchronizationTimeOut));
  EXPECT_TRUE(read_notification.WaitForNotificationWithTimeout(
      kSynchronizationTimeOut));
  EXPECT_TRUE(disconnect_notification.WaitForNotificationWithTimeout(
      kSynchronizationTimeOut));

  EXPECT_FALSE(nearby_connections_manager_->GetRawAuthenticationToken(
      kRemoteEndpointId));
}

TEST_F(NearbyConnectionsManagerImplTest, ConnectSendPayload) {
  // StartDiscovery will succeed.
  NearbyConnectionsService::DiscoveryListener discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  // RequestConnection will succeed.
  NearbyConnectionsService::ConnectionListener connection_listener_remote;
  NearbyConnectionsService::PayloadListener payload_listener_remote;
  Connect(connection_listener_remote, payload_listener_remote,
          ConnectionResponse::kAccepted);

  auto payload_listener =
      std::make_shared<testing::NiceMock<MockPayloadStatusListener>>();
  SendPayload(kPayloadId, payload_listener);

  PayloadTransferUpdate expected_update(kPayloadId, PayloadStatus::kInProgress,
                                        kTotalSize, kBytesTransferred);
  absl::Notification payload_notification;
  EXPECT_CALL(*payload_listener, OnStatusUpdate)
      .WillOnce([&](std::unique_ptr<PayloadTransferUpdate> update,
                    std::optional<Medium> upgraded_medium) {
        EXPECT_EQ(update->payload_id, expected_update.payload_id);
        EXPECT_EQ(update->bytes_transferred, expected_update.bytes_transferred);
        EXPECT_EQ(update->total_bytes, expected_update.total_bytes);
        EXPECT_EQ(update->status, expected_update.status);
        EXPECT_FALSE(upgraded_medium.has_value());
        payload_notification.Notify();
      });

  payload_listener_remote.payload_progress_cb(kRemoteEndpointId,
                                              expected_update);
  EXPECT_TRUE(payload_notification.WaitForNotificationWithTimeout(
      kSynchronizationTimeOut));
}

TEST_F(NearbyConnectionsManagerImplTest, ConnectCancelPayload) {
  // StartDiscovery will succeed.
  NearbyConnectionsService::DiscoveryListener discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  // RequestConnection will succeed.
  NearbyConnectionsService::ConnectionListener connection_listener_remote;
  NearbyConnectionsService::PayloadListener payload_listener_remote;
  Connect(connection_listener_remote, payload_listener_remote,
          ConnectionResponse::kAccepted);

  auto payload_listener =
      std::make_shared<testing::NiceMock<MockPayloadStatusListener>>();
  SendPayload(kPayloadId, payload_listener);

  absl::Notification cancel_notification;
  EXPECT_CALL(*nearby_connections_, CancelPayload)
      .WillOnce([&](absl::string_view service_id, int64_t payload_id,
                    std::function<void(Status status)> callback) {
        EXPECT_EQ(service_id, kServiceId);
        EXPECT_EQ(payload_id, kPayloadId);

        std::move(callback)(Status::kSuccess);
        cancel_notification.Notify();
      });

  absl::Notification payload_notification;
  EXPECT_CALL(*payload_listener, OnStatusUpdate)
      .WillOnce([&](std::unique_ptr<PayloadTransferUpdate> update,
                    std::optional<Medium> upgraded_medium) {
        EXPECT_EQ(update->payload_id, kPayloadId);
        EXPECT_EQ(update->status, PayloadStatus::kCanceled);
        EXPECT_EQ(update->total_bytes, 0u);
        EXPECT_EQ(update->bytes_transferred, 0u);
        EXPECT_FALSE(upgraded_medium.has_value());
        payload_notification.Notify();
      });

  nearby_connections_manager_->Cancel(kPayloadId);
  EXPECT_TRUE(payload_notification.WaitForNotificationWithTimeout(
      kSynchronizationTimeOut));
  EXPECT_TRUE(cancel_notification.WaitForNotificationWithTimeout(
      kSynchronizationTimeOut));
}

TEST_F(NearbyConnectionsManagerImplTest,
       ConnectCancelPayload_MultiplePayloads_HandleDestroyedPayloadListener) {
  // StartDiscovery will succeed.
  NearbyConnectionsService::DiscoveryListener discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  // RequestConnection will succeed.
  NearbyConnectionsService::ConnectionListener connection_listener_remote;
  NearbyConnectionsService::PayloadListener payload_listener_remote;
  Connect(connection_listener_remote, payload_listener_remote,
          ConnectionResponse::kAccepted);

  // Send two payloads with the same listener. We will eventually cancel both
  // payloads, but we will reset the listener before cancelling the second
  // payload. This can happen in practice: if the first payload is cancelled or
  // fails, it makes sense to clean everything up before waiting for the other
  // payload cancellation/failure signals. We are testing that the
  // connection manager handles the missing listener gracefully.
  auto payload_listener =
      std::make_shared<testing::NiceMock<MockPayloadStatusListener>>();
  SendPayload(kPayloadId, payload_listener);
  SendPayload(kPayloadId2, payload_listener);

  absl::Notification cancel_notification;
  EXPECT_CALL(*nearby_connections_, CancelPayload)
      .WillOnce([&](absl::string_view service_id, int64_t payload_id,
                    std::function<void(Status status)> callback) {
        EXPECT_EQ(service_id, kServiceId);
        EXPECT_EQ(payload_id, kPayloadId);

        std::move(callback)(Status::kSuccess);
      })
      .WillOnce([&](absl::string_view service_id, int64_t payload_id,
                    std::function<void(Status status)> callback) {
        EXPECT_EQ(service_id, kServiceId);
        EXPECT_EQ(payload_id, kPayloadId2);

        std::move(callback)(Status::kSuccess);
        cancel_notification.Notify();
      });

  // Because the payload listener is reset before the second payload is
  // cancelled, we can only receive the first status update.
  absl::Notification payload_notification;
  EXPECT_CALL(*payload_listener, OnStatusUpdate)
      .Times(1)
      .WillOnce([&](std::unique_ptr<PayloadTransferUpdate> update,
                    std::optional<Medium> upgraded_medium) {
        EXPECT_EQ(update->payload_id, kPayloadId);
        EXPECT_EQ(update->status, PayloadStatus::kCanceled);
        EXPECT_EQ(update->total_bytes, 0u);
        EXPECT_EQ(update->bytes_transferred, 0u);
        EXPECT_FALSE(upgraded_medium.has_value());

        // Destroy the PayloadStatusListener after the first payload is
        // cancelled.
        payload_listener.reset();

        payload_notification.Notify();
      });

  nearby_connections_manager_->Cancel(kPayloadId);
  nearby_connections_manager_->Cancel(kPayloadId2);
  EXPECT_TRUE(payload_notification.WaitForNotificationWithTimeout(
      kSynchronizationTimeOut));
  EXPECT_TRUE(cancel_notification.WaitForNotificationWithTimeout(
      kSynchronizationTimeOut));
}

TEST_F(NearbyConnectionsManagerImplTest, ConnectTimeout) {
  NearbyConnectionsService::DiscoveryListener discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  // RequestConnection will time out.
  const std::vector<uint8_t> local_endpoint_info(std::begin(kEndpointInfo),
                                                 std::end(kEndpointInfo));

  NearbyConnectionsService::ConnectionListener connection_listener_remote;
  std::function<void(Status status)> connect_callback;
  EXPECT_CALL(*nearby_connections_, RequestConnection)
      .WillOnce([&](absl::string_view service_id,
                    const std::vector<uint8_t>& endpoint_info,
                    absl::string_view endpoint_id,
                    ConnectionOptions connection_options,
                    NearbyConnectionsService::ConnectionListener listener,
                    std::function<void(Status status)> callback) {
        EXPECT_EQ(service_id, kServiceId);
        EXPECT_EQ(endpoint_info, local_endpoint_info);
        EXPECT_EQ(endpoint_id, kRemoteEndpointId);

        connection_listener_remote = std::move(listener);
        // Do not call callback until connection timed out.
        connect_callback = std::move(callback);
      });

  // Timing out should call disconnect.
  EXPECT_CALL(*nearby_connections_, DisconnectFromEndpoint)
      .WillOnce([&](absl::string_view service_id, absl::string_view endpoint_id,
                    std::function<void(Status status)> callback) {
        EXPECT_EQ(service_id, kServiceId);
        EXPECT_EQ(endpoint_id, kRemoteEndpointId);
        std::move(callback)(Status::kSuccess);
      });

  absl::Notification run_notification;
  NearbyConnection* nearby_connection = nullptr;
  nearby_connections_manager_->Connect(
      local_endpoint_info, kRemoteEndpointId,
      /*bluetooth_mac_address=*/std::nullopt, DataUsage::OFFLINE_DATA_USAGE,
      TransportType::kHighQuality,
      [&](NearbyConnection* connection, Status status) {
        nearby_connection = connection;
        run_notification.Notify();
      });
  // Simulate time passing until timeout is reached.
  Fastforward(kInitiateNearbyConnectionTimeout);
  EXPECT_TRUE(
      run_notification.WaitForNotificationWithTimeout(kSynchronizationTimeOut));

  // Expect the callback to be called with a null connection.
  EXPECT_EQ(nearby_connection, nullptr);

  // Resolving connect callback after timeout should do nothing.
  std::move(connect_callback)(Status::kSuccess);
}

TEST_F(NearbyConnectionsManagerImplTest, StartAdvertising) {
  NearbyConnectionsService::ConnectionListener connection_listener_remote;
  testing::NiceMock<MockIncomingConnectionListener>
      incoming_connection_listener;
  StartAdvertising(connection_listener_remote, incoming_connection_listener);

  NearbyConnectionsService::PayloadListener payload_listener_remote;
  NearbyConnection* connection = OnIncomingConnection(
      connection_listener_remote, incoming_connection_listener,
      payload_listener_remote);
  EXPECT_NE(connection, nullptr);
}

TEST_F(NearbyConnectionsManagerImplTest, IncomingPayloadStatusListener) {
  NearbyConnectionsService::ConnectionListener connection_listener_remote;
  testing::NiceMock<MockIncomingConnectionListener>
      incoming_connection_listener;
  StartAdvertising(connection_listener_remote, incoming_connection_listener);

  NearbyConnectionsService::PayloadListener payload_listener_remote;
  NearbyConnection* connection = OnIncomingConnection(
      connection_listener_remote, incoming_connection_listener,
      payload_listener_remote);
  EXPECT_NE(connection, nullptr);

  auto payload_listener =
      std::make_shared<testing::NiceMock<MockPayloadStatusListener>>();
  nearby_connections_manager_->RegisterPayloadStatusListener(
      kPayloadId, payload_listener->GetWeakPtr());

  PayloadTransferUpdate expected_update(kPayloadId, PayloadStatus::kInProgress,
                                        kTotalSize, kBytesTransferred);
  absl::Notification payload_notification;
  EXPECT_CALL(*payload_listener, OnStatusUpdate)
      .WillOnce([&](std::unique_ptr<PayloadTransferUpdate> update,
                    std::optional<Medium> upgraded_medium) {
        EXPECT_THAT(*update, FieldsAre(kPayloadId, PayloadStatus::kInProgress,
                                       kTotalSize, kBytesTransferred));
        EXPECT_FALSE(upgraded_medium.has_value());
        payload_notification.Notify();
      });

  payload_listener_remote.payload_progress_cb(kRemoteEndpointId,
                                              expected_update);
  EXPECT_TRUE(payload_notification.WaitForNotificationWithTimeout(
      kSynchronizationTimeOut));

  // After success status, send another progress update.
  absl::Notification payload_notification_2;
  EXPECT_CALL(*payload_listener, OnStatusUpdate)
      .WillOnce([&](std::unique_ptr<PayloadTransferUpdate> update,
                    std::optional<Medium> upgraded_medium) {
        payload_notification_2.Notify();
      });

  payload_listener_remote.payload_progress_cb(
      kRemoteEndpointId,
      PayloadTransferUpdate(kPayloadId, PayloadStatus::kSuccess, kTotalSize,
                            /*bytes_transferred=*/kTotalSize));
  EXPECT_TRUE(payload_notification_2.WaitForNotificationWithTimeout(
      kSynchronizationTimeOut));

  // PayloadStatusListener will be unregistered and won't receive further
  // updates.
  payload_listener_remote.payload_progress_cb(
      kRemoteEndpointId,
      PayloadTransferUpdate(kPayloadId, PayloadStatus::kSuccess, kTotalSize,
                            /*bytes_transferred=*/kTotalSize));
  EXPECT_CALL(*payload_listener, OnStatusUpdate).Times(0);

  payload_listener_remote.payload_progress_cb(
      kRemoteEndpointId,
      PayloadTransferUpdate(kPayloadId, PayloadStatus::kSuccess, kTotalSize,
                            /*bytes_transferred=*/kTotalSize));
}

TEST_F(NearbyConnectionsManagerImplTest,
       IncomingPayloadStatusListener_MultiplePayloads_HandleDestroyedListener) {
  NearbyConnectionsService::ConnectionListener connection_listener_remote;
  testing::NiceMock<MockIncomingConnectionListener>
      incoming_connection_listener;
  StartAdvertising(connection_listener_remote, incoming_connection_listener);

  NearbyConnectionsService::PayloadListener payload_listener_remote;
  NearbyConnection* connection = OnIncomingConnection(
      connection_listener_remote, incoming_connection_listener,
      payload_listener_remote);
  EXPECT_NE(connection, nullptr);

  // Register three payloads with the same listener. This happens when multiple
  // payloads are included in the same transfer. Use both file and byte payloads
  // to ensure control-frame logic is not invoked for either.
  auto payload_listener =
      std::make_shared<testing::NiceMock<MockPayloadStatusListener>>();
  nearby_connections_manager_->RegisterPayloadStatusListener(
      kPayloadId, payload_listener->GetWeakPtr());
  nearby_connections_manager_->RegisterPayloadStatusListener(
      kPayloadId2, payload_listener->GetWeakPtr());
  nearby_connections_manager_->RegisterPayloadStatusListener(
      kPayloadId3, payload_listener->GetWeakPtr());

  std::filesystem::path file1(std::filesystem::temp_directory_path() /
                              "file1.jpg");
  std::filesystem::path file2(std::filesystem::temp_directory_path() /
                              "file2.jpg");

  InitializeTemporaryFile(file1);
  InitializeTemporaryFile(file2);

  payload_listener_remote.payload_cb(kRemoteEndpointId,
                                     Payload(kPayloadId, InputFile(file1)));
  payload_listener_remote.payload_cb(kRemoteEndpointId,
                                     Payload(kPayloadId2, InputFile(file2)));

  const std::vector<uint8_t> byte_payload(std::begin(kBytePayload),
                                          std::end(kBytePayload));

  payload_listener_remote.payload_cb(
      kRemoteEndpointId, Payload(kPayloadId3, std::move(byte_payload)));

  // Fail the first payload and destroy the payload listener. Then, send updates
  // that the second and third payloads succeeded; this is unlikely in practice,
  // but we test to ensure that no control-frame logic is exercised. Expect that
  // a status update is only sent for the first payload failure because
  // the listener does not exist afterwards.
  absl::Notification payload_notification;
  EXPECT_CALL(*payload_listener, OnStatusUpdate)
      .Times(1)
      .WillOnce([&](std::unique_ptr<PayloadTransferUpdate> update,
                    std::optional<Medium> upgraded_medium) {
        EXPECT_EQ(update->payload_id, kPayloadId);
        EXPECT_EQ(update->status, PayloadStatus::kFailure);
        EXPECT_EQ(update->total_bytes, kTotalSize);
        EXPECT_EQ(update->bytes_transferred, 0u);
        EXPECT_FALSE(upgraded_medium.has_value());

        // Destroy the PayloadStatusListener after the first payload fails.
        payload_listener.reset();
        payload_notification.Notify();
      });
  // Ensure that no control-frame logic is run, which can happen when a payload
  // update is received for an unregistered payload.
  EXPECT_CALL(*nearby_connections_, CancelPayload).Times(0);

  payload_listener_remote.payload_progress_cb(
      kRemoteEndpointId,
      PayloadTransferUpdate(kPayloadId, PayloadStatus::kFailure, kTotalSize,
                            /*bytes_transferred=*/0u));
  payload_listener_remote.payload_progress_cb(
      kRemoteEndpointId,
      PayloadTransferUpdate(kPayloadId2, PayloadStatus::kSuccess, kTotalSize,
                            /*bytes_transferred=*/0u));
  payload_listener_remote.payload_progress_cb(
      kRemoteEndpointId,
      PayloadTransferUpdate(kPayloadId3, PayloadStatus::kSuccess, kTotalSize,
                            /*bytes_transferred=*/0u));
  EXPECT_TRUE(payload_notification.WaitForNotificationWithTimeout(
      kSynchronizationTimeOut));
}

TEST_F(NearbyConnectionsManagerImplTest, IncomingBytesPayload) {
  NearbyConnectionsService::ConnectionListener connection_listener_remote;
  testing::NiceMock<MockIncomingConnectionListener>
      incoming_connection_listener;
  StartAdvertising(connection_listener_remote, incoming_connection_listener);

  NearbyConnectionsService::PayloadListener payload_listener_remote;
  ASSERT_TRUE(OnIncomingConnection(
      connection_listener_remote, incoming_connection_listener,
      payload_listener_remote) != nullptr);

  auto payload_listener =
      std::make_shared<testing::NiceMock<MockPayloadStatusListener>>();
  nearby_connections_manager_->RegisterPayloadStatusListener(
      kPayloadId, payload_listener->GetWeakPtr());

  const std::vector<uint8_t> expected_payload(std::begin(kPayload),
                                              std::end(kPayload));

  payload_listener_remote.payload_cb(kRemoteEndpointId,
                                     Payload(kPayloadId, expected_payload));

  absl::Notification payload_notification;
  EXPECT_CALL(*payload_listener, OnStatusUpdate(::testing::_, ::testing::_))
      .WillOnce([&]() { payload_notification.Notify(); });

  payload_listener_remote.payload_progress_cb(
      kRemoteEndpointId,
      PayloadTransferUpdate(kPayloadId, PayloadStatus::kSuccess, kTotalSize,
                            /*bytes_transferred=*/kTotalSize));
  EXPECT_TRUE(payload_notification.WaitForNotificationWithTimeout(
      kSynchronizationTimeOut));

  const Payload* payload =
      nearby_connections_manager_->GetIncomingPayload(kPayloadId);
  ASSERT_NE(payload, nullptr);
  ASSERT_TRUE(payload->content.is_bytes());
  EXPECT_EQ(payload->content.bytes_payload.bytes, expected_payload);
}

TEST_F(NearbyConnectionsManagerImplTest, IncomingFilePayload) {
  NearbyConnectionsService::ConnectionListener connection_listener_remote;
  testing::NiceMock<MockIncomingConnectionListener>
      incoming_connection_listener;
  StartAdvertising(connection_listener_remote, incoming_connection_listener);

  NearbyConnectionsService::PayloadListener payload_listener_remote;
  NearbyConnection* connection = OnIncomingConnection(
      connection_listener_remote, incoming_connection_listener,
      payload_listener_remote);
  EXPECT_NE(connection, nullptr);

  auto payload_listener =
      std::make_shared<testing::NiceMock<MockPayloadStatusListener>>();
  nearby_connections_manager_->RegisterPayloadStatusListener(
      kPayloadId, payload_listener->GetWeakPtr());

  const std::vector<uint8_t> expected_payload(std::begin(kPayload),
                                              std::end(kPayload));

  std::filesystem::path file(std::filesystem::temp_directory_path() /
                             "file.jpg");
  InitializeTemporaryFile(file);

  payload_listener_remote.payload_cb(kRemoteEndpointId,
                                     Payload(kPayloadId, InputFile(file)));

  absl::Notification payload_notification;
  EXPECT_CALL(*payload_listener, OnStatusUpdate(::testing::_, ::testing::_))
      .WillOnce([&]() { payload_notification.Notify(); });

  payload_listener_remote.payload_progress_cb(
      kRemoteEndpointId,
      PayloadTransferUpdate(kPayloadId, PayloadStatus::kSuccess, kTotalSize,
                            /*bytes_transferred=*/kTotalSize));
  EXPECT_TRUE(payload_notification.WaitForNotificationWithTimeout(
      kSynchronizationTimeOut));

  const Payload* payload =
      nearby_connections_manager_->GetIncomingPayload(kPayloadId);
  ASSERT_NE(payload, nullptr);
  ASSERT_TRUE(payload->content.is_file());
  std::vector<uint8_t> payload_bytes(payload->content.file_payload.size);
  std::FILE* payload_fp = std::fopen(
      payload->content.file_payload.file.path.string().c_str(), "rb");
  ASSERT_NE(payload_fp, nullptr);
  EXPECT_EQ(std::fread(payload_bytes.data(), 1,
                       payload->content.file_payload.size, payload_fp),
            payload->content.file_payload.size);
  std::fclose(payload_fp);
  EXPECT_EQ(payload_bytes, expected_payload);
}

TEST_F(NearbyConnectionsManagerImplTest, ClearIncomingPayloads) {
  NearbyConnectionsService::ConnectionListener connection_listener_remote;
  testing::NiceMock<MockIncomingConnectionListener>
      incoming_connection_listener;
  StartAdvertising(connection_listener_remote, incoming_connection_listener);

  NearbyConnectionsService::PayloadListener payload_listener_remote;
  NearbyConnection* connection = OnIncomingConnection(
      connection_listener_remote, incoming_connection_listener,
      payload_listener_remote);
  EXPECT_NE(connection, nullptr);

  auto payload_listener =
      std::make_shared<testing::NiceMock<MockPayloadStatusListener>>();
  nearby_connections_manager_->RegisterPayloadStatusListener(
      kPayloadId, payload_listener->GetWeakPtr());

  std::filesystem::path file(std::filesystem::temp_directory_path() /
                             "file.jpg");
  InitializeTemporaryFile(file);

  payload_listener_remote.payload_cb(kRemoteEndpointId,
                                     Payload(kPayloadId, InputFile(file)));

  absl::Notification payload_notification;
  EXPECT_CALL(*payload_listener, OnStatusUpdate(::testing::_, ::testing::_))
      .WillOnce([&]() { payload_notification.Notify(); });

  payload_listener_remote.payload_progress_cb(
      kRemoteEndpointId,
      PayloadTransferUpdate(kPayloadId, PayloadStatus::kSuccess, kTotalSize,
                            /*bytes_transferred=*/kTotalSize));
  EXPECT_TRUE(payload_notification.WaitForNotificationWithTimeout(
      kSynchronizationTimeOut));

  nearby_connections_manager_->ClearIncomingPayloads();

  EXPECT_EQ(nearby_connections_manager_->GetIncomingPayload(kPayloadId),
            nullptr);
}

/******************************************************************************/
// Begin: NearbyConnectionsManagerImplTestMediums
/******************************************************************************/
using MediumsTestParam =
    std::tuple<PowerLevel, DataUsage, ConnectivityManager::ConnectionType, bool,
               bool>;
class NearbyConnectionsManagerImplTestMediums
    : public NearbyConnectionsManagerImplTest,
      public testing::WithParamInterface<MediumsTestParam> {};

TEST_P(NearbyConnectionsManagerImplTestMediums, StartAdvertising_Options) {
  const MediumsTestParam& param = GetParam();
  PowerLevel power_level = std::get<0>(param);
  DataUsage data_usage = std::get<1>(param);
  ConnectivityManager::ConnectionType connection_type = std::get<2>(param);
  bool is_webrtc_enabled = std::get<3>(GetParam());
  bool is_wifilan_enabled = std::get<4>(GetParam());

  if (is_webrtc_enabled) {
    NearbyFlags::GetInstance().OverrideBoolFlagValue(
        config_package_nearby::nearby_sharing_feature::kEnableMediumWebRtc,
        true);
  } else {
    NearbyFlags::GetInstance().OverrideBoolFlagValue(
        config_package_nearby::nearby_sharing_feature::kEnableMediumWebRtc,
        false);
  }
  if (is_wifilan_enabled) {
    NearbyFlags::GetInstance().OverrideBoolFlagValue(
        config_package_nearby::nearby_sharing_feature::kEnableMediumWifiLan,
        true);
  } else {
    NearbyFlags::GetInstance().OverrideBoolFlagValue(
        config_package_nearby::nearby_sharing_feature::kEnableMediumWifiLan,
        false);
  }

  SetConnectionType(connection_type);

  bool should_use_internet =
      data_usage != DataUsage::OFFLINE_DATA_USAGE &&
      connection_type != ConnectivityManager::ConnectionType::kNone &&
      !(data_usage == DataUsage::WIFI_ONLY_DATA_USAGE &&
        connection_type != ConnectivityManager::ConnectionType::kWifi);
  bool is_connection_wifi_or_ethernet =
      connection_type == ConnectivityManager::ConnectionType::kWifi ||
      connection_type == ConnectivityManager::ConnectionType::kEthernet;
  should_use_web_rtc_ = is_webrtc_enabled && should_use_internet;
  should_use_wifilan_ = is_wifilan_enabled & is_connection_wifi_or_ethernet;

  bool is_high_power = power_level == PowerLevel::kHighPower;

  MediumSelection expected_mediums(
      /*bluetooth=*/is_high_power,
      /*ble=*/true,
      /*web_rtc=*/should_use_web_rtc_,
      /*wifi_lan=*/should_use_wifilan_,
      /*wifi_hotspot=*/true);

  absl::Notification notification;
  const std::vector<uint8_t> local_endpoint_info(std::begin(kEndpointInfo),
                                                 std::end(kEndpointInfo));
  testing::NiceMock<MockIncomingConnectionListener>
      incoming_connection_listener;

  NearbyConnectionsManager::ConnectionsCallback callback = [&](Status status) {
    EXPECT_EQ(status, Status::kSuccess);
    notification.Notify();
  };

  EXPECT_CALL(*nearby_connections_, StartAdvertising)
      .WillOnce(
          [&](absl::string_view service_id,
              const std::vector<uint8_t>& endpoint_info,
              AdvertisingOptions options,
              NearbyConnectionsService::ConnectionListener advertising_listener,
              std::function<void(Status status)> callback) {
            EXPECT_EQ(options.auto_upgrade_bandwidth, false);
            EXPECT_EQ(options.allowed_mediums.ble, expected_mediums.ble);
            EXPECT_EQ(options.allowed_mediums.bluetooth,
                      expected_mediums.bluetooth);
            EXPECT_EQ(options.allowed_mediums.web_rtc,
                      expected_mediums.web_rtc);
            EXPECT_EQ(options.allowed_mediums.wifi_lan,
                      expected_mediums.wifi_lan);
            EXPECT_EQ(options.enable_bluetooth_listening, true);
            EXPECT_EQ(options.enable_webrtc_listening,
                      is_high_power && should_use_web_rtc_);
            std::move(callback)(Status::kSuccess);
          });

  nearby_connections_manager_->StartAdvertising(
      local_endpoint_info, &incoming_connection_listener, power_level,
      data_usage, false, std::move(callback));

  EXPECT_TRUE(
      notification.WaitForNotificationWithTimeout(kSynchronizationTimeOut));
}

INSTANTIATE_TEST_SUITE_P(
    NearbyConnectionsManagerImplTestMediums,
    NearbyConnectionsManagerImplTestMediums,
    testing::Combine(testing::Values(PowerLevel::kLowPower,
                                     PowerLevel::kHighPower),
                     testing::Values(DataUsage::WIFI_ONLY_DATA_USAGE,
                                     DataUsage::OFFLINE_DATA_USAGE,
                                     DataUsage::ONLINE_DATA_USAGE),
                     testing::Values(ConnectivityManager::ConnectionType::kNone,
                                     ConnectivityManager::ConnectionType::kWifi,
                                     ConnectivityManager::ConnectionType::k3G),
                     testing::Bool(), testing::Bool()));

/******************************************************************************/
// End: NearbyConnectionsManagerImplTestMediums
/******************************************************************************/

TEST_F(NearbyConnectionsManagerImplTest, StopAdvertising) {
  NearbyConnectionsService::ConnectionListener connection_listener_remote;
  testing::NiceMock<MockIncomingConnectionListener>
      incoming_connection_listener;
  StartAdvertising(connection_listener_remote, incoming_connection_listener);

  absl::Notification notification;
  EXPECT_CALL(*nearby_connections_, StopAdvertising)
      .WillOnce([&](absl::string_view service_id,
                    std::function<void(Status status)> callback) {
        EXPECT_EQ(service_id, kServiceId);
        std::move(callback)(Status::kSuccess);
        notification.Notify();
      });
  nearby_connections_manager_->StopAdvertising(
      [](Status status) { EXPECT_EQ(status, Status::kSuccess); });
  EXPECT_TRUE(
      notification.WaitForNotificationWithTimeout(kSynchronizationTimeOut));
}

TEST_F(NearbyConnectionsManagerImplTest, ShutdownAdvertising) {
  NearbyConnectionsService::ConnectionListener connection_listener_remote;
  testing::NiceMock<MockIncomingConnectionListener>
      incoming_connection_listener;
  StartAdvertising(connection_listener_remote, incoming_connection_listener);

  absl::Notification notification;
  EXPECT_CALL(*nearby_connections_, StopAllEndpoints)
      .WillOnce([&](std::function<void(Status status)> callback) {
        std::move(callback)(Status::kSuccess);
        notification.Notify();
      });
  nearby_connections_manager_->Shutdown();
  EXPECT_TRUE(
      notification.WaitForNotificationWithTimeout(kSynchronizationTimeOut));
}

TEST_F(NearbyConnectionsManagerImplTest,
       UpgradeBandwidthAfterAdvertisingSucceeds) {
  NearbyConnectionsService::ConnectionListener connection_listener_remote;
  testing::NiceMock<MockIncomingConnectionListener>
      incoming_connection_listener;
  StartAdvertising(connection_listener_remote, incoming_connection_listener);

  // Upgrading bandwidth will succeed.
  absl::Notification notification;
  EXPECT_CALL(*nearby_connections_, InitiateBandwidthUpgrade)
      .WillOnce([&](absl::string_view service_id, absl::string_view endpoint_id,
                    std::function<void(Status status)> callback) {
        EXPECT_EQ(service_id, kServiceId);
        EXPECT_EQ(endpoint_id, kRemoteEndpointId);
        std::move(callback)(Status::kSuccess);
        notification.Notify();
      });
  nearby_connections_manager_->UpgradeBandwidth(kRemoteEndpointId);
  EXPECT_TRUE(
      notification.WaitForNotificationWithTimeout(kSynchronizationTimeOut));
}

TEST_F(NearbyConnectionsManagerImplTest,
       UpgradeBandwidthAfterDiscoverySucceeds) {
  // StartDiscovery will succeed.
  NearbyConnectionsService::DiscoveryListener discovery_listener_remote;
  testing::NiceMock<MockDiscoveryListener> discovery_listener;
  StartDiscovery(discovery_listener_remote, discovery_listener);

  // RequestConnection will succeed.
  NearbyConnectionsService::ConnectionListener connection_listener_remote;
  NearbyConnectionsService::PayloadListener payload_listener_remote;
  NearbyConnection* nearby_connection =
      Connect(connection_listener_remote, payload_listener_remote,
              ConnectionResponse::kAccepted);
  EXPECT_NE(nearby_connection, nullptr);

  // Upgrading bandwidth will succeed.
  absl::Notification notification;
  EXPECT_CALL(*nearby_connections_, InitiateBandwidthUpgrade)
      .WillOnce([&](absl::string_view service_id, absl::string_view endpoint_id,
                    std::function<void(Status status)> callback) {
        EXPECT_EQ(service_id, kServiceId);
        EXPECT_EQ(endpoint_id, kRemoteEndpointId);
        std::move(callback)(Status::kSuccess);
        notification.Notify();
      });
  nearby_connections_manager_->UpgradeBandwidth(kRemoteEndpointId);
  EXPECT_TRUE(
      notification.WaitForNotificationWithTimeout(kSynchronizationTimeOut));
}

TEST_F(NearbyConnectionsManagerImplTest, UnknownFilePathsToDelete) {
  nearby_connections_manager_->AddUnknownFilePathsToDeleteForTesting(
      "test1.txt");
  nearby_connections_manager_->AddUnknownFilePathsToDeleteForTesting(
      "test2.txt");
  auto unknown_file_paths =
      nearby_connections_manager_->GetUnknownFilePathsToDeleteForTesting();
  nearby_connections_manager_->AddUnknownFilePathsToDeleteForTesting(
      "test3.txt");

  // Test if we get copy of container.
  EXPECT_NE(unknown_file_paths.size(), 3);
  EXPECT_EQ(unknown_file_paths.size(), 2);
  EXPECT_EQ(nearby_connections_manager_->GetUnknownFilePathsToDeleteForTesting()
                .size(),
            3);
  unknown_file_paths =
      nearby_connections_manager_->GetUnknownFilePathsToDeleteForTesting();
  EXPECT_THAT(unknown_file_paths,
              UnorderedElementsAre("test1.txt", "test2.txt", "test3.txt"));
  nearby_connections_manager_->GetAndClearUnknownFilePathsToDelete();
  EXPECT_TRUE(
      nearby_connections_manager_->GetUnknownFilePathsToDeleteForTesting()
          .empty());

  // Test GetAndClearUnknownFilePathsToDelete
  nearby_connections_manager_->AddUnknownFilePathsToDeleteForTesting(
      "test1.txt");
  nearby_connections_manager_->AddUnknownFilePathsToDeleteForTesting(
      "test2.txt");
  unknown_file_paths =
      nearby_connections_manager_->GetAndClearUnknownFilePathsToDelete();
  EXPECT_EQ(unknown_file_paths.size(), 2);
  EXPECT_TRUE(
      nearby_connections_manager_->GetUnknownFilePathsToDeleteForTesting()
          .empty());
}

TEST_F(NearbyConnectionsManagerImplTest,
       OnPayloadTransferUpdateForUnknownFile) {
  NearbyConnectionsService::ConnectionListener connection_listener_remote;
  testing::NiceMock<MockIncomingConnectionListener>
      incoming_connection_listener;
  StartAdvertising(connection_listener_remote, incoming_connection_listener);

  NearbyConnectionsService::PayloadListener payload_listener_remote;
  ASSERT_TRUE(OnIncomingConnection(
      connection_listener_remote, incoming_connection_listener,
      payload_listener_remote) != nullptr);
  std::filesystem::path file(std::filesystem::temp_directory_path() /
                             "file.jpg");
  payload_listener_remote.payload_cb(kRemoteEndpointId,
                                     Payload(kPayloadId, InputFile(file)));

  nearby_connections_manager_->OnPayloadTransferUpdateForTesting(
      kRemoteEndpointId,
      PayloadTransferUpdate(kPayloadId, PayloadStatus::kCanceled, kTotalSize,
                            /*bytes_transferred=*/kTotalSize));
  absl::flat_hash_set<std::filesystem::path> unknown_file_paths =
      nearby_connections_manager_->GetUnknownFilePathsToDeleteForTesting();
  EXPECT_EQ(unknown_file_paths.size(), 1);
  nearby_connections_manager_->GetAndClearUnknownFilePathsToDelete();

  // Flag is on. Don't add unknown file paths w/o kCanceled to the list.
  nearby_connections_manager_->OnPayloadTransferUpdateForTesting(
      kRemoteEndpointId,
      PayloadTransferUpdate(kPayloadId, PayloadStatus::kFailure, kTotalSize,
                            /*bytes_transferred=*/kTotalSize));
  unknown_file_paths =
      nearby_connections_manager_->GetUnknownFilePathsToDeleteForTesting();
  EXPECT_TRUE(unknown_file_paths.empty());
}

TEST_F(NearbyConnectionsManagerImplTest,
       OnPayloadReceivedForUnknownFile) {
  NearbyConnectionsService::ConnectionListener connection_listener_remote;
  testing::NiceMock<MockIncomingConnectionListener>
      incoming_connection_listener;
  StartAdvertising(connection_listener_remote, incoming_connection_listener);

  NearbyConnectionsService::PayloadListener payload_listener_remote;
  ASSERT_TRUE(OnIncomingConnection(
      connection_listener_remote, incoming_connection_listener,
      payload_listener_remote) != nullptr);
  std::filesystem::path file(std::filesystem::temp_directory_path() /
                             "file.jpg");
  payload_listener_remote.payload_cb(kRemoteEndpointId,
                                     Payload(kPayloadId, InputFile(file)));

  // Flag is on. Add unknown file paths with kCanceled to the list.
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      config_package_nearby::nearby_sharing_feature::
          kDeleteUnexpectedReceivedFileFix,
      true);
  nearby_connections_manager_->ClearIncomingPayloads();
  Payload payload(kPayloadId, InputFile(file));
  nearby_connections_manager_->OnPayloadReceivedForTesting(
      kRemoteEndpointId, payload);

  std::filesystem::path file2(std::filesystem::temp_directory_path() /
                             "file2.jpg");
  Payload payload2(kPayloadId, InputFile(file2));
  nearby_connections_manager_->OnPayloadReceivedForTesting(
      kRemoteEndpointId, payload2);
  auto unknown_file_paths =
      nearby_connections_manager_->GetAndClearUnknownFilePathsToDelete();

  EXPECT_EQ(unknown_file_paths.size(), 2);

  auto payload_listener =
      std::make_shared<testing::NiceMock<MockPayloadStatusListener>>();
  nearby_connections_manager_->RegisterPayloadStatusListener(
      kPayloadId, payload_listener->GetWeakPtr());
  std::filesystem::path file3(std::filesystem::temp_directory_path() /
                             "file3.jpg");
  Payload payload3(kPayloadId, InputFile(file3));
  nearby_connections_manager_->OnPayloadReceivedForTesting(
      kRemoteEndpointId, payload3);
  unknown_file_paths =
      nearby_connections_manager_->GetUnknownFilePathsToDeleteForTesting();

  EXPECT_EQ(unknown_file_paths.size(), 0);
}

TEST_F(NearbyConnectionsManagerImplTest,
       OnPayloadReceivedDeletePreviousFileWithSamePayloadId) {
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      config_package_nearby::nearby_sharing_feature::
          kDeleteUnexpectedReceivedFileFix,
      true);

  NearbyConnectionsService::ConnectionListener connection_listener_remote;
  testing::NiceMock<MockIncomingConnectionListener>
      incoming_connection_listener;
  StartAdvertising(connection_listener_remote, incoming_connection_listener);

  NearbyConnectionsService::PayloadListener payload_listener_remote;
  ASSERT_TRUE(OnIncomingConnection(
      connection_listener_remote, incoming_connection_listener,
      payload_listener_remote) != nullptr);
  auto payload_listener =
      std::make_shared<testing::NiceMock<MockPayloadStatusListener>>();
  nearby_connections_manager_->RegisterPayloadStatusListener(
      kPayloadId, payload_listener->GetWeakPtr());

  std::filesystem::path file(std::filesystem::temp_directory_path() /
                             "file.jpg");

  Payload payload(kPayloadId, InputFile(file));
  nearby_connections_manager_->OnPayloadReceivedForTesting(
      kRemoteEndpointId, payload);

  auto unknown_file_paths =
      nearby_connections_manager_->GetUnknownFilePathsToDeleteForTesting();
  EXPECT_EQ(unknown_file_paths.size(), 0);

  absl::Notification cancel_notification;
  EXPECT_CALL(*nearby_connections_, CancelPayload)
      .WillOnce([&](absl::string_view service_id, int64_t payload_id,
                    std::function<void(Status status)> callback) {
        EXPECT_EQ(service_id, kServiceId);
        EXPECT_EQ(payload_id, kPayloadId);

        std::move(callback)(Status::kSuccess);
        cancel_notification.Notify();
      });

  absl::Notification payload_notification;
  EXPECT_CALL(*payload_listener, OnStatusUpdate)
      .WillOnce([&](std::unique_ptr<PayloadTransferUpdate> update,
                    std::optional<Medium> upgraded_medium) {
        EXPECT_EQ(update->payload_id, kPayloadId);
        EXPECT_EQ(update->status, PayloadStatus::kCanceled);
        EXPECT_EQ(update->total_bytes, 0u);
        EXPECT_EQ(update->bytes_transferred, 0u);
        EXPECT_FALSE(upgraded_medium.has_value());
        payload_notification.Notify();
      });

  std::filesystem::path file2(std::filesystem::temp_directory_path() /
                              "file2.jpg");
  Payload payload2(kPayloadId, InputFile(file2));
  nearby_connections_manager_->OnPayloadReceivedForTesting(kRemoteEndpointId,
                                                           payload2);
  unknown_file_paths =
      nearby_connections_manager_->GetAndClearUnknownFilePathsToDelete();
  EXPECT_EQ(unknown_file_paths.size(), 1);

  EXPECT_TRUE(payload_notification.WaitForNotificationWithTimeout(
      kSynchronizationTimeOut));
  EXPECT_TRUE(cancel_notification.WaitForNotificationWithTimeout(
      kSynchronizationTimeOut));
}

TEST_F(NearbyConnectionsManagerImplTest, ProcessUnknownFilePathsToDelete) {
  std::filesystem::path file(std::filesystem::temp_directory_path() /
                             "file.jpg");
  nearby_connections_manager_->ProcessUnknownFilePathsToDeleteForTesting(
      PayloadStatus::kCanceled, PayloadContent::Type::kFile, file);
  absl::flat_hash_set<std::filesystem::path> unknown_file_paths =
      nearby_connections_manager_->GetUnknownFilePathsToDeleteForTesting();
  EXPECT_EQ(unknown_file_paths.size(), 1);
  nearby_connections_manager_->GetAndClearUnknownFilePathsToDelete();

  // Check we add kInProgress status.
  nearby_connections_manager_->ProcessUnknownFilePathsToDeleteForTesting(
      PayloadStatus::kInProgress, PayloadContent::Type::kFile, file);
  unknown_file_paths =
      nearby_connections_manager_->GetUnknownFilePathsToDeleteForTesting();
  EXPECT_EQ(unknown_file_paths.size(), 1);
  nearby_connections_manager_->GetAndClearUnknownFilePathsToDelete();

  // Check only one file is added to the list, since we use hash set.
  nearby_connections_manager_->ProcessUnknownFilePathsToDeleteForTesting(
      PayloadStatus::kInProgress, PayloadContent::Type::kFile, file);
  nearby_connections_manager_->ProcessUnknownFilePathsToDeleteForTesting(
      PayloadStatus::kCanceled, PayloadContent::Type::kFile, file);
  unknown_file_paths =
      nearby_connections_manager_->GetUnknownFilePathsToDeleteForTesting();
  EXPECT_EQ(unknown_file_paths.size(), 1);
  nearby_connections_manager_->GetAndClearUnknownFilePathsToDelete();

  // Check kSuccess or kFailure are not added to the list.
  nearby_connections_manager_->ProcessUnknownFilePathsToDeleteForTesting(
      PayloadStatus::kSuccess, PayloadContent::Type::kFile, file);
  nearby_connections_manager_->ProcessUnknownFilePathsToDeleteForTesting(
      PayloadStatus::kFailure, PayloadContent::Type::kFile, file);
  unknown_file_paths =
      nearby_connections_manager_->GetUnknownFilePathsToDeleteForTesting();
  EXPECT_TRUE(unknown_file_paths.empty());
  nearby_connections_manager_->GetAndClearUnknownFilePathsToDelete();

  // Check only kFile type is added to the list.
  nearby_connections_manager_->ProcessUnknownFilePathsToDeleteForTesting(
      PayloadStatus::kSuccess, PayloadContent::Type::kBytes, file);
  unknown_file_paths =
      nearby_connections_manager_->GetUnknownFilePathsToDeleteForTesting();
  EXPECT_TRUE(unknown_file_paths.empty());
  nearby_connections_manager_->GetAndClearUnknownFilePathsToDelete();

  nearby_connections_manager_->ProcessUnknownFilePathsToDeleteForTesting(
      PayloadStatus::kFailure, PayloadContent::Type::kFile, file);
  unknown_file_paths =
      nearby_connections_manager_->GetUnknownFilePathsToDeleteForTesting();
  EXPECT_TRUE(unknown_file_paths.empty());
  nearby_connections_manager_->GetAndClearUnknownFilePathsToDelete();
}

}  // namespace NearbyConnectionsManagerUnitTests
}  // namespace sharing
}  // namespace nearby
