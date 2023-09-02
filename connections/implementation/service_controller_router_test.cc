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

#include "connections/implementation/service_controller_router.h"

#include <array>
#include <cinttypes>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/types/span.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/mock_service_controller.h"
#include "connections/listeners.h"
#include "connections/params.h"
#include "connections/v3/bandwidth_info.h"
#include "connections/v3/connection_listening_options.h"
#include "connections/v3/connection_result.h"
#include "connections/v3/connections_device.h"
#include "connections/v3/listening_result.h"
#include "connections/v3/params.h"
#include "internal/platform/borrowable.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/condition_variable.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/mutex.h"
#include "internal/platform/mutex_lock.h"

namespace nearby {
namespace connections {

namespace {
using ::testing::Return;
constexpr std::array<char, 6> kFakeMacAddress = {'a', 'b', 'c', 'd', 'e', 'f'};
constexpr std::array<char, 6> kFakeInjectedEndpointInfo = {'g', 'h', 'i'};
const char kFakeInejctedEndpointId[] = "abcd";
}  // namespace

class FakeNearbyDevice : public NearbyDevice {
 public:
  NearbyDevice::Type GetType() const override {
    return NearbyDevice::Type::kUnknownDevice;
  }
  MOCK_METHOD(std::string, GetEndpointId, (), (const override));
  MOCK_METHOD(std::vector<ConnectionInfoVariant>, GetConnectionInfos, (),
              (const override));
  MOCK_METHOD(std::string, ToProtoBytes, (), (const override));
};

// This class must be in the same namespace as ServiceControllerRouter for
// friend class to work.
class ServiceControllerRouterTest : public testing::Test {
 public:
  void SetUp() override {
    auto mock = std::make_unique<MockServiceController>();
    mock_ = mock.get();
    router_.SetServiceControllerForTesting(std::move(mock));
  }

  void StartAdvertising(::nearby::Borrowable<ClientProxy*> client,
                        std::string service_id,
                        AdvertisingOptions advertising_options,
                        ConnectionRequestInfo info, ResultCallback callback) {
    EXPECT_CALL(*mock_, StartAdvertising)
        .WillOnce(Return(Status{Status::kSuccess}));
    {
      MutexLock lock(&mutex_);
      complete_ = false;
      router_.StartAdvertising(client, service_id, advertising_options, info,
                               std::move(callback));
      while (!complete_) cond_.Wait();
      EXPECT_EQ(result_, Status{Status::kSuccess});
    }
    EXPECT_TRUE(client.Borrow());
    (*client.Borrow())
        ->StartedAdvertising(kServiceId, advertising_options.strategy,
                             info.listener, absl::MakeSpan(mediums_));
    EXPECT_TRUE((*client.Borrow())->IsAdvertising());
  }

  void StopAdvertising(::nearby::Borrowable<ClientProxy*> client,
                       ResultCallback callback) {
    EXPECT_CALL(*mock_, StopAdvertising).Times(1);
    {
      MutexLock lock(&mutex_);
      complete_ = false;
      router_.StopAdvertising(client, std::move(callback));
      while (!complete_) cond_.Wait();
    }
    EXPECT_TRUE(client.Borrow());
    (*client.Borrow())->StoppedAdvertising();
    EXPECT_FALSE((*client.Borrow())->IsAdvertising());
  }

  void StartDiscovery(::nearby::Borrowable<ClientProxy*> client,
                      std::string service_id,
                      DiscoveryOptions discovery_options,
                      const DiscoveryListener& listener,
                      ResultCallback callback) {
    EXPECT_CALL(*mock_, StartDiscovery)
        .WillOnce(Return(Status{Status::kSuccess}));
    {
      MutexLock lock(&mutex_);
      complete_ = false;
      router_.StartDiscovery(client, kServiceId, discovery_options, listener,
                             std::move(callback));
      while (!complete_) cond_.Wait();
      EXPECT_EQ(result_, Status{Status::kSuccess});
    }
    EXPECT_TRUE(client.Borrow());
    (*client.Borrow())
        ->StartedDiscovery(service_id, discovery_options.strategy, listener,
                           absl::MakeSpan(mediums_));
    EXPECT_TRUE((*client.Borrow())->IsDiscovering());
  }

  void StopDiscovery(::nearby::Borrowable<ClientProxy*> client,
                     ResultCallback callback) {
    EXPECT_CALL(*mock_, StopDiscovery).Times(1);
    {
      MutexLock lock(&mutex_);
      complete_ = false;
      router_.StopDiscovery(client, std::move(callback));
      while (!complete_) cond_.Wait();
    }
    EXPECT_TRUE(client.Borrow());
    (*client.Borrow())->StoppedDiscovery();
    EXPECT_FALSE((*client.Borrow())->IsDiscovering());
  }

  void InjectEndpoint(::nearby::Borrowable<ClientProxy*> client,
                      std::string service_id,
                      const OutOfBandConnectionMetadata& metadata,
                      ResultCallback callback) {
    EXPECT_CALL(*mock_, InjectEndpoint).Times(1);
    {
      MutexLock lock(&mutex_);
      complete_ = false;
      router_.InjectEndpoint(client, service_id, metadata, std::move(callback));
      while (!complete_) cond_.Wait();
    }
  }

  void RequestConnection(::nearby::Borrowable<ClientProxy*> client,
                         const std::string& endpoint_id,
                         const ConnectionRequestInfo& request_info,
                         ResultCallback callback) {
    EXPECT_CALL(*mock_, RequestConnection)
        .WillOnce(Return(Status{Status::kSuccess}));
    ConnectionOptions connection_options;
    {
      MutexLock lock(&mutex_);
      complete_ = false;
      router_.RequestConnection(client, endpoint_id, request_info,
                                connection_options, std::move(callback));
      while (!complete_) cond_.Wait();
      EXPECT_EQ(result_, Status{Status::kSuccess});
    }
    ConnectionResponseInfo response_info{
        .remote_endpoint_info = ByteArray{"endpoint_name"},
        .authentication_token = "auth_token",
        .raw_authentication_token = ByteArray{"auth_token"},
        .is_incoming_connection = true,
    };
    std::string connection_token{"conntokn"};
    EXPECT_TRUE(client.Borrow());
    (*client.Borrow())
        ->OnConnectionInitiated(endpoint_id, response_info, connection_options,
                                request_info.listener, connection_token);
    EXPECT_TRUE(
        (*client.Borrow())->HasPendingConnectionToEndpoint(endpoint_id));
  }

  void AcceptConnection(::nearby::Borrowable<ClientProxy*> client,
                        const std::string endpoint_id,
                        ResultCallback callback) {
    EXPECT_CALL(*mock_, AcceptConnection)
        .WillOnce(Return(Status{Status::kSuccess}));
    // Pre-condition for successful Accept is: connection must exist.
    EXPECT_TRUE(
        (*client.Borrow())->HasPendingConnectionToEndpoint(endpoint_id));
    {
      MutexLock lock(&mutex_);
      complete_ = false;
      router_.AcceptConnection(client, endpoint_id, {}, std::move(callback));
      while (!complete_) cond_.Wait();
      EXPECT_EQ(result_, Status{Status::kSuccess});
    }
    EXPECT_TRUE(client.Borrow());
    (*client.Borrow())->LocalEndpointAcceptedConnection(endpoint_id, {});
    (*client.Borrow())->RemoteEndpointAcceptedConnection(endpoint_id);
    EXPECT_TRUE((*client.Borrow())->IsConnectionAccepted(endpoint_id));
    (*client.Borrow())->OnConnectionAccepted(endpoint_id);
    EXPECT_TRUE((*client.Borrow())->IsConnectedToEndpoint(endpoint_id));
  }

  void RejectConnection(::nearby::Borrowable<ClientProxy*> client,
                        const std::string endpoint_id,
                        ResultCallback callback) {
    EXPECT_CALL(*mock_, RejectConnection)
        .WillOnce(Return(Status{Status::kSuccess}));
    // Pre-condition for successful Accept is: connection must exist.
    EXPECT_TRUE(
        (*client.Borrow())->HasPendingConnectionToEndpoint(endpoint_id));
    {
      MutexLock lock(&mutex_);
      complete_ = false;
      router_.RejectConnection(client, endpoint_id, std::move(callback));
      while (!complete_) cond_.Wait();
      EXPECT_EQ(result_, Status{Status::kSuccess});
    }
    EXPECT_TRUE(client.Borrow());
    (*client.Borrow())->LocalEndpointRejectedConnection(endpoint_id);
    EXPECT_TRUE((*client.Borrow())->IsConnectionRejected(endpoint_id));
  }

  void InitiateBandwidthUpgrade(::nearby::Borrowable<ClientProxy*> client,
                                const std::string endpoint_id,
                                ResultCallback callback) {
    EXPECT_CALL(*mock_, InitiateBandwidthUpgrade).Times(1);
    EXPECT_TRUE(client.Borrow());
    EXPECT_TRUE((*client.Borrow())->IsConnectedToEndpoint(endpoint_id));
    {
      MutexLock lock(&mutex_);
      complete_ = false;
      router_.InitiateBandwidthUpgrade(client, endpoint_id,
                                       std::move(callback));
      while (!complete_) cond_.Wait();
      EXPECT_EQ(result_, Status{Status::kSuccess});
    }
  }

  void SendPayload(::nearby::Borrowable<ClientProxy*> client,
                   const std::vector<std::string>& endpoint_ids,
                   Payload payload, ResultCallback callback) {
    EXPECT_CALL(*mock_, SendPayload).Times(1);

    bool connected = false;
    EXPECT_TRUE(client.Borrow());
    for (const auto& endpoint_id : endpoint_ids) {
      connected =
          connected || (*client.Borrow())->IsConnectedToEndpoint(endpoint_id);
    }
    EXPECT_TRUE(connected);
    {
      MutexLock lock(&mutex_);
      complete_ = false;
      router_.SendPayload(client, absl::MakeSpan(endpoint_ids),
                          std::move(payload), std::move(callback));
      while (!complete_) cond_.Wait();
      EXPECT_EQ(result_, Status{Status::kSuccess});
    }
  }

  void CancelPayload(::nearby::Borrowable<ClientProxy*> client,
                     std::int64_t payload_id, ResultCallback callback) {
    EXPECT_CALL(*mock_, CancelPayload)
        .WillOnce(Return(Status{Status::kSuccess}));
    {
      MutexLock lock(&mutex_);
      complete_ = false;
      router_.CancelPayload(client, payload_id, std::move(callback));
      while (!complete_) cond_.Wait();
      EXPECT_EQ(result_, Status{Status::kSuccess});
    }
  }

  void DisconnectFromEndpoint(::nearby::Borrowable<ClientProxy*> client,
                              const std::string endpoint_id,
                              ResultCallback callback) {
    EXPECT_CALL(*mock_, DisconnectFromEndpoint).Times(1);
    EXPECT_TRUE(client.Borrow());
    EXPECT_TRUE((*client.Borrow())->IsConnectedToEndpoint(endpoint_id));
    {
      MutexLock lock(&mutex_);
      complete_ = false;
      router_.DisconnectFromEndpoint(client, endpoint_id, std::move(callback));
      while (!complete_) cond_.Wait();
    }
    (*client.Borrow())->OnDisconnected(endpoint_id, false);
    EXPECT_FALSE((*client.Borrow())->IsConnectedToEndpoint(endpoint_id));
  }

  void RequestConnectionV3(::nearby::Borrowable<ClientProxy*> client,
                           const NearbyDevice& kRemoteDevice,
                           v3::ConnectionRequestInfo request_info,
                           ResultCallback callback, bool call_all_cb,
                           bool check_result = true,
                           bool endpoint_info_present = true) {
    // If we set check_result to false, we expect that RequestConnection will
    // not be called.
    if (check_result) {
      EXPECT_CALL(*mock_, RequestConnection)
          .WillOnce([call_all_cb, endpoint_info_present, this](
                        ::nearby::Borrowable<ClientProxy*> client,
                        const std::string&, const ConnectionRequestInfo& info,
                        const ConnectionOptions&) {
            EXPECT_EQ(info.endpoint_info.Empty(), !endpoint_info_present);
            if (call_all_cb) {
              info.listener.initiated_cb(kRemoteEndpointId, {});
              info.listener.accepted_cb(kRemoteEndpointId);
              info.listener.rejected_cb(kRemoteEndpointId,
                                        Status{Status::kConnectionRejected});
              info.listener.disconnected_cb(kRemoteEndpointId);
              info.listener.bandwidth_changed_cb(kRemoteEndpointId,
                                                 Medium::BLUETOOTH);
            }
            return Status{Status::kSuccess};
          });
    }
    ConnectionOptions connection_options;
    {
      MutexLock lock(&mutex_);
      complete_ = false;
      router_.RequestConnectionV3(client, kRemoteDevice,
                                  std::move(request_info), connection_options,
                                  std::move(callback));
      while (!complete_) cond_.Wait();
      if (check_result) {
        EXPECT_EQ(result_, Status{Status::kSuccess});
      }
    }
    ConnectionResponseInfo response_info{
        .remote_endpoint_info = ByteArray{"endpoint_name"},
        .authentication_token = "auth_token",
        .raw_authentication_token = ByteArray{"auth_token"},
        .is_incoming_connection = true,
    };

    EXPECT_TRUE(client.Borrow());
    if ((*client.Borrow())
            ->HasPendingConnectionToEndpoint(kRemoteDevice.GetEndpointId())) {
      // we are calling this again, and do not need to rerun the below behavior.
      return;
    }
    (*client.Borrow())
        ->OnConnectionInitiated(kRemoteDevice.GetEndpointId(), response_info,
                                connection_options, {}, "conntokn");
    EXPECT_TRUE(
        (*client.Borrow())
            ->HasPendingConnectionToEndpoint(kRemoteDevice.GetEndpointId()));
  }

  void AcceptConnectionV3(::nearby::Borrowable<ClientProxy*> client,
                          const NearbyDevice& kRemoteDevice,
                          ResultCallback callback) {
    EXPECT_CALL(*mock_, AcceptConnection)
        .WillOnce(Return(Status{Status::kSuccess}));
    // Pre-condition for successful Accept is: connection must exist.
    EXPECT_TRUE(client.Borrow());
    EXPECT_TRUE(
        (*client.Borrow())
            ->HasPendingConnectionToEndpoint(kRemoteDevice.GetEndpointId()));
    {
      MutexLock lock(&mutex_);
      complete_ = false;
      router_.AcceptConnectionV3(client, kRemoteDevice, {},
                                 std::move(callback));
      while (!complete_) cond_.Wait();
      EXPECT_EQ(result_, Status{Status::kSuccess});
    }
    auto endpoint_id = kRemoteDevice.GetEndpointId();
    (*client.Borrow())->LocalEndpointAcceptedConnection(endpoint_id, {});
    (*client.Borrow())->RemoteEndpointAcceptedConnection(endpoint_id);
    EXPECT_TRUE((*client.Borrow())->IsConnectionAccepted(endpoint_id));
    (*client.Borrow())->OnConnectionAccepted(endpoint_id);
    EXPECT_TRUE((*client.Borrow())->IsConnectedToEndpoint(endpoint_id));
  }

  void RejectConnectionV3(::nearby::Borrowable<ClientProxy*> client,
                          const NearbyDevice& device, ResultCallback callback) {
    EXPECT_CALL(*mock_, RejectConnection)
        .WillOnce(Return(Status{Status::kSuccess}));
    // Pre-condition for successful Accept is: connection must exist.
    EXPECT_TRUE(client.Borrow());
    EXPECT_TRUE((*client.Borrow())
                    ->HasPendingConnectionToEndpoint(device.GetEndpointId()));
    {
      MutexLock lock(&mutex_);
      complete_ = false;
      router_.RejectConnectionV3(client, device, std::move(callback));
      while (!complete_) cond_.Wait();
      EXPECT_EQ(result_, Status{Status::kSuccess});
    }
    (*client.Borrow())->LocalEndpointRejectedConnection(device.GetEndpointId());
    EXPECT_TRUE(
        (*client.Borrow())->IsConnectionRejected(device.GetEndpointId()));
  }

  void InitiateBandwidthUpgradeV3(::nearby::Borrowable<ClientProxy*> client,
                                  const NearbyDevice& device,
                                  ResultCallback callback) {
    EXPECT_CALL(*mock_, InitiateBandwidthUpgrade).Times(1);
    EXPECT_TRUE(client.Borrow());
    EXPECT_TRUE(
        (*client.Borrow())->IsConnectedToEndpoint(device.GetEndpointId()));
    {
      MutexLock lock(&mutex_);
      complete_ = false;
      router_.InitiateBandwidthUpgradeV3(client, device, std::move(callback));
      while (!complete_) cond_.Wait();
      EXPECT_EQ(result_, Status{Status::kSuccess});
    }
  }

  void SendPayloadV3(::nearby::Borrowable<ClientProxy*> client,
                     const NearbyDevice& recipient_device, Payload payload,
                     ResultCallback callback) {
    EXPECT_CALL(*mock_, SendPayload).Times(1);
    EXPECT_TRUE(client.Borrow());

    bool connected =
        (*client.Borrow())
            ->IsConnectedToEndpoint(recipient_device.GetEndpointId());
    EXPECT_TRUE(connected);
    {
      MutexLock lock(&mutex_);
      complete_ = false;
      router_.SendPayloadV3(client, recipient_device, std::move(payload),
                            std::move(callback));
      while (!complete_) cond_.Wait();
      EXPECT_EQ(result_, Status{Status::kSuccess});
    }
  }

  void CancelPayloadV3(::nearby::Borrowable<ClientProxy*> client,
                       const NearbyDevice& recipient_device,
                       uint64_t payload_id, ResultCallback callback) {
    EXPECT_CALL(*mock_, CancelPayload).Times(1);
    EXPECT_TRUE(client.Borrow());
    EXPECT_TRUE((*client.Borrow())
                    ->IsConnectedToEndpoint(recipient_device.GetEndpointId()));
    {
      MutexLock lock(&mutex_);
      router_.CancelPayloadV3(client, recipient_device, payload_id,
                              std::move(callback));
    }
  }

  void DisconnectFromDeviceV3(::nearby::Borrowable<ClientProxy*> client,
                              const NearbyDevice& kRemoteDevice,
                              ResultCallback callback) {
    EXPECT_CALL(*mock_, DisconnectFromEndpoint).Times(1);
    EXPECT_TRUE(client.Borrow());
    EXPECT_TRUE((*client.Borrow())
                    ->IsConnectedToEndpoint(kRemoteDevice.GetEndpointId()));
    {
      MutexLock lock(&mutex_);
      complete_ = false;
      router_.DisconnectFromDeviceV3(client, kRemoteDevice,
                                     std::move(callback));
      while (!complete_) cond_.Wait();
    }
    (*client.Borrow())->OnDisconnected(kRemoteDevice.GetEndpointId(), false);
    EXPECT_FALSE((*client.Borrow())
                     ->IsConnectedToEndpoint(kRemoteDevice.GetEndpointId()));
  }

  void StartListeningForIncomingConnectionsV3(
      ::nearby::Borrowable<ClientProxy*> client, absl::string_view service_id,
      v3::ConnectionListener listener,
      const v3::ConnectionListeningOptions& options,
      v3::ListeningResultListener result_listener, bool expecting_call = true) {
    EXPECT_TRUE(client.Borrow());
    if (expecting_call) {
      EXPECT_CALL(*mock_, StartListeningForIncomingConnections)
          .Times(1)
          .WillOnce([](::nearby::Borrowable<ClientProxy*> client,
                       absl::string_view service_id, v3::ConnectionListener,
                       const v3::ConnectionListeningOptions& options) {
            (*client.Borrow())
                ->StartedListeningForIncomingConnections(
                    service_id, options.strategy, {}, options);
            return std::pair<Status, std::vector<ConnectionInfoVariant>>{
                Status{Status::kSuccess}, {}};
          });
    }
    {
      MutexLock lock(&mutex_);
      complete_ = false;
      router_.StartListeningForIncomingConnectionsV3(
          client, service_id, std::move(listener), options,
          std::move(result_listener));
      while (!complete_) cond_.Wait();
      EXPECT_TRUE((*client.Borrow())->IsListeningForIncomingConnections());
    }
  }

 protected:
  const std::string kServiceId = "service id";
  const std::string kRequestorName = "requestor name";
  const std::string kRemoteEndpointId = "remote endpoint id";
  const v3::ConnectionsDevice kRemoteDevice =
      v3::ConnectionsDevice(kRemoteEndpointId, "", {});
  const std::int64_t kPayloadId = UINT64_C(0x123456789ABCDEF0);
  const ConnectionOptions kConnectionOptions{
      {
          Strategy::kP2pPointToPoint,
          BooleanMediumSelector(),
      },
      true,
      true,
  };
  const AdvertisingOptions kAdvertisingOptions{
      {
          Strategy::kP2pPointToPoint,
          BooleanMediumSelector(),
      },
      true,   // auto_upgrade_bandwidth
      true,   // enforce_topology_constraints
      false,  // low_power
      false,  // enable_bluetooth_listening
      false,  // enable_webrtc_listening
  };
  const DiscoveryOptions kDiscoveryOptions{
      {
          Strategy::kP2pPointToPoint,
          BooleanMediumSelector(),
      },
      true,
      true,
  };

  const OutOfBandConnectionMetadata kOutOfBandConnectionMetadata{
      .medium = Medium::BLUETOOTH,
      .endpoint_id = kFakeInejctedEndpointId,
      .endpoint_info = ByteArray(kFakeInjectedEndpointInfo),
      .remote_bluetooth_mac_address = ByteArray(kFakeMacAddress),
  };

  std::vector<location::nearby::proto::connections::Medium> mediums_{
      location::nearby::proto::connections::Medium::BLUETOOTH};
  const ConnectionRequestInfo kConnectionRequestInfo{
      .endpoint_info = ByteArray{kRequestorName},
      .listener = ConnectionListener(),
  };

  DiscoveryListener discovery_listener_;

  Mutex mutex_;
  ConditionVariable cond_{&mutex_};
  Status result_ ABSL_GUARDED_BY(mutex_) = {Status::kError};
  bool complete_ ABSL_GUARDED_BY(mutex_) = false;
  MockServiceController* mock_;
  ClientProxy client_;

  ServiceControllerRouter router_;
};

namespace {
TEST_F(ServiceControllerRouterTest, QualityConversionWorks) {
  EXPECT_EQ(router_.GetMediumQuality(Medium::UNKNOWN_MEDIUM),
            v3::Quality::kUnknown);
  EXPECT_EQ(router_.GetMediumQuality(Medium::USB), v3::Quality::kUnknown);
  EXPECT_EQ(router_.GetMediumQuality(Medium::BLE), v3::Quality::kLow);
  EXPECT_EQ(router_.GetMediumQuality(Medium::NFC), v3::Quality::kLow);
  EXPECT_EQ(router_.GetMediumQuality(Medium::BLUETOOTH), v3::Quality::kMedium);
  EXPECT_EQ(router_.GetMediumQuality(Medium::BLE_L2CAP), v3::Quality::kMedium);
  EXPECT_EQ(router_.GetMediumQuality(Medium::WEB_RTC), v3::Quality::kHigh);
  EXPECT_EQ(router_.GetMediumQuality(Medium::WIFI_HOTSPOT), v3::Quality::kHigh);
  EXPECT_EQ(router_.GetMediumQuality(Medium::WIFI_LAN), v3::Quality::kHigh);
  EXPECT_EQ(router_.GetMediumQuality(Medium::WIFI_DIRECT), v3::Quality::kHigh);
  EXPECT_EQ(router_.GetMediumQuality(Medium::WIFI_AWARE), v3::Quality::kHigh);
}

TEST_F(ServiceControllerRouterTest, StartAdvertisingCalled) {
  StartAdvertising(client_.GetBorrowable(), kServiceId, kAdvertisingOptions,
                   kConnectionRequestInfo, [this](Status status) {
                     MutexLock lock(&mutex_);
                     result_ = status;
                     complete_ = true;
                     cond_.Notify();
                   });
}

TEST_F(ServiceControllerRouterTest, StopAdvertisingCalled) {
  StartAdvertising(client_.GetBorrowable(), kServiceId, kAdvertisingOptions,
                   kConnectionRequestInfo, [this](Status status) {
                     MutexLock lock(&mutex_);
                     result_ = status;
                     complete_ = true;
                     cond_.Notify();
                   });
  StopAdvertising(client_.GetBorrowable(), [this](Status status) {
    MutexLock lock(&mutex_);
    result_ = status;
    complete_ = true;
    cond_.Notify();
  });
}

TEST_F(ServiceControllerRouterTest, StartDiscoveryCalled) {
  StartDiscovery(client_.GetBorrowable(), kServiceId, kDiscoveryOptions,
                 discovery_listener_, [this](Status status) {
                   MutexLock lock(&mutex_);
                   result_ = status;
                   complete_ = true;
                   cond_.Notify();
                 });
}

TEST_F(ServiceControllerRouterTest, StopDiscoveryCalled) {
  StartDiscovery(client_.GetBorrowable(), kServiceId, kDiscoveryOptions,
                 discovery_listener_, [this](Status status) {
                   MutexLock lock(&mutex_);
                   result_ = status;
                   complete_ = true;
                   cond_.Notify();
                 });
  StopDiscovery(client_.GetBorrowable(), [this](Status status) {
    MutexLock lock(&mutex_);
    result_ = status;
    complete_ = true;
    cond_.Notify();
  });
}

TEST_F(ServiceControllerRouterTest, InjectEndpointCalled) {
  StartDiscovery(client_.GetBorrowable(), kServiceId, kDiscoveryOptions,
                 discovery_listener_, [this](Status status) {
                   MutexLock lock(&mutex_);
                   result_ = status;
                   complete_ = true;
                   cond_.Notify();
                 });
  InjectEndpoint(client_.GetBorrowable(), kServiceId,
                 kOutOfBandConnectionMetadata, [this](Status status) {
                   MutexLock lock(&mutex_);
                   result_ = status;
                   complete_ = true;
                   cond_.Notify();
                 });
  StopDiscovery(client_.GetBorrowable(), [this](Status status) {
    MutexLock lock(&mutex_);
    result_ = status;
    complete_ = true;
    cond_.Notify();
  });
}

TEST_F(ServiceControllerRouterTest, RequestConnectionCalled) {
  // Either Advertising, or Discovery should be ongoing.
  StartDiscovery(client_.GetBorrowable(), kServiceId, kDiscoveryOptions,
                 discovery_listener_, [this](Status status) {
                   MutexLock lock(&mutex_);
                   result_ = status;
                   complete_ = true;
                   cond_.Notify();
                 });
  RequestConnection(client_.GetBorrowable(), kRemoteEndpointId,
                    kConnectionRequestInfo, [this](Status status) {
                      MutexLock lock(&mutex_);
                      result_ = status;
                      complete_ = true;
                      cond_.Notify();
                    });
}

TEST_F(ServiceControllerRouterTest, AcceptConnectionCalled) {
  // Either Advertising, or Discovery should be ongoing.
  StartDiscovery(client_.GetBorrowable(), kServiceId, kDiscoveryOptions,
                 discovery_listener_, [this](Status status) {
                   MutexLock lock(&mutex_);
                   result_ = status;
                   complete_ = true;
                   cond_.Notify();
                 });
  // Establish connection.
  RequestConnection(client_.GetBorrowable(), kRemoteEndpointId,
                    kConnectionRequestInfo, [this](Status status) {
                      MutexLock lock(&mutex_);
                      result_ = status;
                      complete_ = true;
                      cond_.Notify();
                    });
  // Now, we can accept connection.
  AcceptConnection(client_.GetBorrowable(), kRemoteEndpointId,
                   [this](Status status) {
                     MutexLock lock(&mutex_);
                     result_ = status;
                     complete_ = true;
                     cond_.Notify();
                   });
}

TEST_F(ServiceControllerRouterTest, RejectConnectionCalled) {
  // Either Advertising, or Discovery should be ongoing.
  StartDiscovery(client_.GetBorrowable(), kServiceId, kDiscoveryOptions,
                 discovery_listener_, [this](Status status) {
                   MutexLock lock(&mutex_);
                   result_ = status;
                   complete_ = true;
                   cond_.Notify();
                 });
  // Establish connection.
  RequestConnection(client_.GetBorrowable(), kRemoteEndpointId,
                    kConnectionRequestInfo, [this](Status status) {
                      MutexLock lock(&mutex_);
                      result_ = status;
                      complete_ = true;
                      cond_.Notify();
                    });
  // Now, we can reject connection.
  RejectConnection(client_.GetBorrowable(), kRemoteEndpointId,
                   [this](Status status) {
                     MutexLock lock(&mutex_);
                     result_ = status;
                     complete_ = true;
                     cond_.Notify();
                   });
}

TEST_F(ServiceControllerRouterTest, InitiateBandwidthUpgradeCalled) {
  // Either Advertising, or Discovery should be ongoing.
  StartDiscovery(client_.GetBorrowable(), kServiceId, kDiscoveryOptions,
                 discovery_listener_, [this](Status status) {
                   MutexLock lock(&mutex_);
                   result_ = status;
                   complete_ = true;
                   cond_.Notify();
                 });
  // Establish connection.
  RequestConnection(client_.GetBorrowable(), kRemoteEndpointId,
                    kConnectionRequestInfo, [this](Status status) {
                      MutexLock lock(&mutex_);
                      result_ = status;
                      complete_ = true;
                      cond_.Notify();
                    });
  // Now, we can accept connection.
  AcceptConnection(client_.GetBorrowable(), kRemoteEndpointId,
                   [this](Status status) {
                     MutexLock lock(&mutex_);
                     result_ = status;
                     complete_ = true;
                     cond_.Notify();
                   });
  // Now we can change connection bandwidth.
  InitiateBandwidthUpgrade(client_.GetBorrowable(), kRemoteEndpointId,
                           [this](Status status) {
                             MutexLock lock(&mutex_);
                             result_ = status;
                             complete_ = true;
                             cond_.Notify();
                           });
}

TEST_F(ServiceControllerRouterTest, SendPayloadCalled) {
  // Either Advertising, or Discovery should be ongoing.
  StartDiscovery(client_.GetBorrowable(), kServiceId, kDiscoveryOptions,
                 discovery_listener_, [this](Status status) {
                   MutexLock lock(&mutex_);
                   result_ = status;
                   complete_ = true;
                   cond_.Notify();
                 });
  // Establish connection.
  RequestConnection(client_.GetBorrowable(), kRemoteEndpointId,
                    kConnectionRequestInfo, [this](Status status) {
                      MutexLock lock(&mutex_);
                      result_ = status;
                      complete_ = true;
                      cond_.Notify();
                    });
  // Now, we can accept connection.
  AcceptConnection(client_.GetBorrowable(), kRemoteEndpointId,
                   [this](Status status) {
                     MutexLock lock(&mutex_);
                     result_ = status;
                     complete_ = true;
                     cond_.Notify();
                   });
  // Now we can send payload.
  SendPayload(client_.GetBorrowable(),
              std::vector<std::string>{kRemoteEndpointId},
              Payload{ByteArray("data")}, [this](Status status) {
                MutexLock lock(&mutex_);
                result_ = status;
                complete_ = true;
                cond_.Notify();
              });
}

TEST_F(ServiceControllerRouterTest, CancelPayloadCalled) {
  // Either Advertising, or Discovery should be ongoing.
  StartDiscovery(client_.GetBorrowable(), kServiceId, kDiscoveryOptions,
                 discovery_listener_, [this](Status status) {
                   MutexLock lock(&mutex_);
                   result_ = status;
                   complete_ = true;
                   cond_.Notify();
                 });
  // Establish connection.
  RequestConnection(client_.GetBorrowable(), kRemoteEndpointId,
                    kConnectionRequestInfo, [this](Status status) {
                      MutexLock lock(&mutex_);
                      result_ = status;
                      complete_ = true;
                      cond_.Notify();
                    });
  // Now, we can accept connection.
  AcceptConnection(client_.GetBorrowable(), kRemoteEndpointId,
                   [this](Status status) {
                     MutexLock lock(&mutex_);
                     result_ = status;
                     complete_ = true;
                     cond_.Notify();
                   });
  // We have to know payload id, before we can cancel payload transfer.
  // It is either after a call to SendPayload, or after receiving
  // PayloadProgress callback. Let's assume we have it, and proceed.
  CancelPayload(client_.GetBorrowable(), kPayloadId, [this](Status status) {
    MutexLock lock(&mutex_);
    result_ = status;
    complete_ = true;
    cond_.Notify();
  });
}

TEST_F(ServiceControllerRouterTest, DisconnectFromEndpointCalled) {
  // Either Advertising, or Discovery should be ongoing.
  StartDiscovery(client_.GetBorrowable(), kServiceId, kDiscoveryOptions,
                 discovery_listener_, [this](Status status) {
                   MutexLock lock(&mutex_);
                   result_ = status;
                   complete_ = true;
                   cond_.Notify();
                 });
  // Establish connection.
  RequestConnection(client_.GetBorrowable(), kRemoteEndpointId,
                    kConnectionRequestInfo, [this](Status status) {
                      MutexLock lock(&mutex_);
                      result_ = status;
                      complete_ = true;
                      cond_.Notify();
                    });
  // Now, we can accept connection.
  AcceptConnection(client_.GetBorrowable(), kRemoteEndpointId,
                   [this](Status status) {
                     MutexLock lock(&mutex_);
                     result_ = status;
                     complete_ = true;
                     cond_.Notify();
                   });
  // We can disconnect at any time after RequestConnection.
  DisconnectFromEndpoint(client_.GetBorrowable(), kRemoteEndpointId,
                         [this](Status status) {
                           MutexLock lock(&mutex_);
                           result_ = status;
                           complete_ = true;
                           cond_.Notify();
                         });
}

TEST_F(ServiceControllerRouterTest, RequestConnectionCalledV3) {
  // Either Advertising, or Discovery should be ongoing.
  StartDiscovery(client_.GetBorrowable(), kServiceId, kDiscoveryOptions,
                 discovery_listener_, [this](Status status) {
                   MutexLock lock(&mutex_);
                   result_ = status;
                   complete_ = true;
                   cond_.Notify();
                 });
  // Establish connection.
  auto local_device =
      v3::ConnectionsDevice(client_.GetLocalEndpointId(), kRequestorName, {});
  // Testing callback wrapping as well.
  CountDownLatch initiated_latch(1);
  CountDownLatch result_latch(2);
  CountDownLatch disconnected_latch(1);
  CountDownLatch bandwidth_changed_latch(1);
  RequestConnectionV3(
      client_.GetBorrowable(), kRemoteDevice,
      v3::ConnectionRequestInfo{
          .local_device = local_device,
          .listener = {
              .initiated_cb =
                  [&initiated_latch](const NearbyDevice&,
                                     const v3::InitialConnectionInfo&) {
                    initiated_latch.CountDown();
                  },
              .result_cb =
                  [&result_latch](const NearbyDevice&, v3::ConnectionResult) {
                    result_latch.CountDown();
                  },
              .disconnected_cb =
                  [&disconnected_latch](const NearbyDevice&) {
                    disconnected_latch.CountDown();
                  },
              .bandwidth_changed_cb =
                  [&bandwidth_changed_latch](const NearbyDevice&,
                                             v3::BandwidthInfo) {
                    bandwidth_changed_latch.CountDown();
                  }},
      },
      [this](Status status) {
        MutexLock lock(&mutex_);
        result_ = status;
        complete_ = true;
        cond_.Notify();
      },
      true);
  EXPECT_TRUE(initiated_latch.Await().Ok());
  EXPECT_TRUE(result_latch.Await().Ok());
  EXPECT_TRUE(disconnected_latch.Await().Ok());
  EXPECT_TRUE(bandwidth_changed_latch.Await().Ok());
}

TEST_F(ServiceControllerRouterTest, RequestConnectionV3FakeDevice) {
  // Either Advertising, or Discovery should be ongoing.
  StartDiscovery(client_.GetBorrowable(), kServiceId, kDiscoveryOptions,
                 discovery_listener_, [this](Status status) {
                   MutexLock lock(&mutex_);
                   result_ = status;
                   complete_ = true;
                   cond_.Notify();
                 });
  // Establish connection.
  auto local_device = FakeNearbyDevice();
  // Testing callback wrapping as well.
  CountDownLatch initiated_latch(1);
  CountDownLatch result_latch(2);
  CountDownLatch disconnected_latch(1);
  CountDownLatch bandwidth_changed_latch(1);
  RequestConnectionV3(
      client_.GetBorrowable(), kRemoteDevice,
      v3::ConnectionRequestInfo{
          .local_device = local_device,
          .listener = {
              .initiated_cb =
                  [&initiated_latch](const NearbyDevice&,
                                     const v3::InitialConnectionInfo&) {
                    initiated_latch.CountDown();
                  },
              .result_cb =
                  [&result_latch](const NearbyDevice&, v3::ConnectionResult) {
                    result_latch.CountDown();
                  },
              .disconnected_cb =
                  [&disconnected_latch](const NearbyDevice&) {
                    disconnected_latch.CountDown();
                  },
              .bandwidth_changed_cb =
                  [&bandwidth_changed_latch](const NearbyDevice&,
                                             v3::BandwidthInfo) {
                    bandwidth_changed_latch.CountDown();
                  }},
      },
      [this](Status status) {
        MutexLock lock(&mutex_);
        result_ = status;
        complete_ = true;
        cond_.Notify();
      },
      true, true, false);
  EXPECT_TRUE(initiated_latch.Await().Ok());
  EXPECT_TRUE(result_latch.Await().Ok());
  EXPECT_TRUE(disconnected_latch.Await().Ok());
  EXPECT_TRUE(bandwidth_changed_latch.Await().Ok());
}

TEST_F(ServiceControllerRouterTest, RequestConnectionV3TwiceFails) {
  // Either Advertising, or Discovery should be ongoing.
  StartDiscovery(client_.GetBorrowable(), kServiceId, kDiscoveryOptions,
                 discovery_listener_, [this](Status status) {
                   MutexLock lock(&mutex_);
                   result_ = status;
                   complete_ = true;
                   cond_.Notify();
                 });
  // Establish connection.
  auto local_device =
      v3::ConnectionsDevice(client_.GetLocalEndpointId(), kRequestorName, {});
  RequestConnectionV3(
      client_.GetBorrowable(), kRemoteDevice,
      v3::ConnectionRequestInfo{
          .local_device = local_device,
          .listener = {},
      },
      [this](Status status) {
        MutexLock lock(&mutex_);
        result_ = status;
        complete_ = true;
        cond_.Notify();
      },
      false);
  RequestConnectionV3(
      client_.GetBorrowable(), kRemoteDevice,
      v3::ConnectionRequestInfo{
          .local_device = local_device,
          .listener = {},
      },
      [this](Status status) {
        MutexLock lock(&mutex_);
        result_ = status;
        complete_ = true;
        cond_.Notify();
      },
      false, false);
  {
    MutexLock lock(&mutex_);
    EXPECT_EQ(result_.value, Status::kAlreadyConnectedToEndpoint);
  }
}

TEST_F(ServiceControllerRouterTest, AcceptConnectionCalledV3) {
  // Either Advertising, or Discovery should be ongoing.
  StartDiscovery(client_.GetBorrowable(), kServiceId, kDiscoveryOptions,
                 discovery_listener_, [this](Status status) {
                   MutexLock lock(&mutex_);
                   result_ = status;
                   complete_ = true;
                   cond_.Notify();
                 });
  // Establish connection.
  auto local_device =
      v3::ConnectionsDevice(client_.GetLocalEndpointId(), kRequestorName, {});
  RequestConnectionV3(
      client_.GetBorrowable(), kRemoteDevice,
      v3::ConnectionRequestInfo{
          .local_device = local_device,
          .listener = {},
      },
      [this](Status status) {
        MutexLock lock(&mutex_);
        result_ = status;
        complete_ = true;
        cond_.Notify();
      },
      false);
  // Now, we can accept connection.
  AcceptConnectionV3(client_.GetBorrowable(), kRemoteDevice,
                     [this](Status status) {
                       MutexLock lock(&mutex_);
                       result_ = status;
                       complete_ = true;
                       cond_.Notify();
                     });
}

TEST_F(ServiceControllerRouterTest, RejectConnectionCalledV3) {
  // Either Advertising, or Discovery should be ongoing.
  StartDiscovery(client_.GetBorrowable(), kServiceId, kDiscoveryOptions,
                 discovery_listener_, [this](Status status) {
                   MutexLock lock(&mutex_);
                   result_ = status;
                   complete_ = true;
                   cond_.Notify();
                 });
  // Establish connection.
  auto local_device =
      v3::ConnectionsDevice(client_.GetLocalEndpointId(), kRequestorName, {});
  RequestConnectionV3(
      client_.GetBorrowable(), kRemoteDevice,
      v3::ConnectionRequestInfo{
          .local_device = local_device,
          .listener = {},
      },
      [this](Status status) {
        MutexLock lock(&mutex_);
        result_ = status;
        complete_ = true;
        cond_.Notify();
      },
      false);
  // Now, we can reject connection.
  RejectConnectionV3(client_.GetBorrowable(), kRemoteDevice,
                     [this](Status status) {
                       MutexLock lock(&mutex_);
                       result_ = status;
                       complete_ = true;
                       cond_.Notify();
                     });
}

TEST_F(ServiceControllerRouterTest, InitiateBandwidthUpgradeCalledV3) {
  // Either Advertising, or Discovery should be ongoing.
  StartDiscovery(client_.GetBorrowable(), kServiceId, kDiscoveryOptions,
                 discovery_listener_, [this](Status status) {
                   MutexLock lock(&mutex_);
                   result_ = status;
                   complete_ = true;
                   cond_.Notify();
                 });
  // Establish connection.
  auto local_device =
      v3::ConnectionsDevice(client_.GetLocalEndpointId(), kRequestorName, {});
  RequestConnectionV3(
      client_.GetBorrowable(), kRemoteDevice,
      v3::ConnectionRequestInfo{
          .local_device = local_device,
          .listener = {},
      },
      [this](Status status) {
        MutexLock lock(&mutex_);
        result_ = status;
        complete_ = true;
        cond_.Notify();
      },
      false);
  // Now, we can accept connection.
  AcceptConnectionV3(client_.GetBorrowable(), kRemoteDevice,
                     [this](Status status) {
                       MutexLock lock(&mutex_);
                       result_ = status;
                       complete_ = true;
                       cond_.Notify();
                     });
  // Now we can change connection bandwidth.
  InitiateBandwidthUpgradeV3(client_.GetBorrowable(), kRemoteDevice,
                             [this](Status status) {
                               MutexLock lock(&mutex_);
                               result_ = status;
                               complete_ = true;
                               cond_.Notify();
                             });
}

TEST_F(ServiceControllerRouterTest, SendPayloadCalledV3) {
  // Either Advertising, or Discovery should be ongoing.
  StartDiscovery(client_.GetBorrowable(), kServiceId, kDiscoveryOptions,
                 discovery_listener_, [this](Status status) {
                   MutexLock lock(&mutex_);
                   result_ = status;
                   complete_ = true;
                   cond_.Notify();
                 });
  // Establish connection.
  auto local_device =
      v3::ConnectionsDevice(client_.GetLocalEndpointId(), kRequestorName, {});
  RequestConnectionV3(
      client_.GetBorrowable(), kRemoteDevice,
      v3::ConnectionRequestInfo{
          .local_device = local_device,
          .listener = {},
      },
      [this](Status status) {
        MutexLock lock(&mutex_);
        result_ = status;
        complete_ = true;
        cond_.Notify();
      },
      false);
  // Now, we can accept connection.
  AcceptConnectionV3(client_.GetBorrowable(), kRemoteDevice,
                     [this](Status status) {
                       MutexLock lock(&mutex_);
                       result_ = status;
                       complete_ = true;
                       cond_.Notify();
                     });
  // Now we can send payload.
  SendPayloadV3(client_.GetBorrowable(), kRemoteDevice,
                Payload{ByteArray("data")}, [this](Status status) {
                  MutexLock lock(&mutex_);
                  result_ = status;
                  complete_ = true;
                  cond_.Notify();
                });
}

TEST_F(ServiceControllerRouterTest, DisconnectFromDeviceCalledV3) {
  // Either Advertising, or Discovery should be ongoing.
  StartDiscovery(client_.GetBorrowable(), kServiceId, kDiscoveryOptions,
                 discovery_listener_, [this](Status status) {
                   MutexLock lock(&mutex_);
                   result_ = status;
                   complete_ = true;
                   cond_.Notify();
                 });
  // Establish connection.
  auto local_device =
      v3::ConnectionsDevice(client_.GetLocalEndpointId(), kRequestorName, {});
  RequestConnectionV3(
      client_.GetBorrowable(), kRemoteDevice,
      v3::ConnectionRequestInfo{
          .local_device = local_device,
          .listener = {},
      },
      [this](Status status) {
        MutexLock lock(&mutex_);
        result_ = status;
        complete_ = true;
        cond_.Notify();
      },
      false);
  // Now, we can accept connection.
  AcceptConnectionV3(client_.GetBorrowable(), kRemoteDevice,
                     [this](Status status) {
                       MutexLock lock(&mutex_);
                       result_ = status;
                       complete_ = true;
                       cond_.Notify();
                     });
  // We can disconnect at any time after RequestConnection.
  DisconnectFromDeviceV3(client_.GetBorrowable(), kRemoteDevice,
                         [this](Status status) {
                           MutexLock lock(&mutex_);
                           result_ = status;
                           complete_ = true;
                           cond_.Notify();
                         });
}

TEST_F(ServiceControllerRouterTest, CancelPayloadV3Called) {
  // Either Advertising, or Discovery should be ongoing.
  StartDiscovery(client_.GetBorrowable(), kServiceId, kDiscoveryOptions,
                 discovery_listener_, [this](Status status) {
                   MutexLock lock(&mutex_);
                   result_ = status;
                   complete_ = true;
                   cond_.Notify();
                 });
  // Establish connection.
  auto local_device =
      v3::ConnectionsDevice(client_.GetLocalEndpointId(), kRequestorName, {});
  RequestConnectionV3(
      client_.GetBorrowable(), kRemoteDevice,
      v3::ConnectionRequestInfo{
          .local_device = local_device,
          .listener = {},
      },
      [this](Status status) {
        MutexLock lock(&mutex_);
        result_ = status;
        complete_ = true;
        cond_.Notify();
      },
      false);
  // Now, we can accept connection.
  AcceptConnectionV3(client_.GetBorrowable(), kRemoteDevice,
                     [this](Status status) {
                       MutexLock lock(&mutex_);
                       result_ = status;
                       complete_ = true;
                       cond_.Notify();
                     });
  // We have to know payload id, before we can cancel payload transfer.
  // It is either after a call to SendPayload, or after receiving
  // PayloadProgress callback. Let's assume we have it, and proceed.
  CancelPayloadV3(client_.GetBorrowable(), kRemoteDevice, kPayloadId,
                  [this](Status status) {
                    MutexLock lock(&mutex_);
                    result_ = status;
                    complete_ = true;
                    cond_.Notify();
                  });
}

TEST_F(ServiceControllerRouterTest,
       StartListeningForIncomingConnectionsCalledV3) {
  StartListeningForIncomingConnectionsV3(
      client_.GetBorrowable(), kServiceId, {}, {},
      [this](std::pair<Status, v3::ListeningResult> result) {
        EXPECT_TRUE(result.first.Ok());
        {
          MutexLock lock(&mutex_);
          complete_ = true;
        }
        cond_.Notify();
      });
}

TEST_F(ServiceControllerRouterTest,
       StartListeningForIncomingConnectionsCalledV3TwiceFails) {
  StartListeningForIncomingConnectionsV3(
      client_.GetBorrowable(), kServiceId, {}, {},
      [this](std::pair<Status, v3::ListeningResult> result) {
        EXPECT_TRUE(result.first.Ok());
        {
          MutexLock lock(&mutex_);
          complete_ = true;
        }
        cond_.Notify();
      });

  StartListeningForIncomingConnectionsV3(
      client_.GetBorrowable(), kServiceId, {}, {},
      [this](std::pair<Status, v3::ListeningResult> result) {
        EXPECT_EQ(result.first.value, Status::kAlreadyListening);
        {
          MutexLock lock(&mutex_);
          complete_ = true;
        }
        cond_.Notify();
      },
      /*expecting_call=*/false);
}

}  // namespace
}  // namespace connections
}  // namespace nearby
