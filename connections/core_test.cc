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

#include "connections/core.h"

#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/blocking_counter.h"
#include "absl/synchronization/notification.h"
#include "connections/advertising_options.h"
#include "connections/discovery_options.h"
#include "connections/implementation/mock_service_controller_router.h"
#include "connections/listeners.h"
#include "connections/medium_selector.h"
#include "connections/params.h"
#include "connections/payload.h"
#include "connections/power_level.h"
#include "connections/status.h"
#include "connections/strategy.h"
#include "connections/v3/advertising_options.h"
#include "connections/v3/bandwidth_info.h"
#include "connections/v3/connection_result.h"
#include "connections/v3/connections_device.h"
#include "connections/v3/discovery_options.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace connections {
namespace {

class FakeNearbyDevice : public NearbyDevice {
 public:
  NearbyDevice::Type GetType() const override {
    return NearbyDevice::Type::kUnknownDevice;
  }
  MOCK_METHOD(std::string, GetEndpointId, (), (const, override));
  MOCK_METHOD(std::vector<ConnectionInfoVariant>, GetConnectionInfos, (),
              (const, override));
  MOCK_METHOD(std::string, ToProtoBytes, (), (const, override));
};

class FakeNearbyDeviceProvider : public NearbyDeviceProvider {
 public:
  const NearbyDevice* GetLocalDevice() override { return &local_device_; }

 private:
  FakeNearbyDevice local_device_;
};

TEST(CoreTest, ConstructorDestructorWorks) {
  MockServiceControllerRouter mock_controller;
  // Called when Core is destroyed.
  EXPECT_CALL(mock_controller, StopAllEndpoints)
      .WillOnce([&](ClientProxy* client, ResultCallback callback) {
        callback({Status::kSuccess});
      });
  Core core{&mock_controller};
}

TEST(CoreTest, DestructorReportsFatalFailure) {
  ASSERT_DEATH(
      {
        MockServiceControllerRouter mock_controller;
        // Never invoke the result callback so ~Core will time out.
        EXPECT_CALL(mock_controller, StopAllEndpoints);
        Core core{&mock_controller};
      },
      "Unable to shutdown");
}

TEST(CoreTest, RequestConnectionCallsScRouter) {
  MockServiceControllerRouter mock_controller;
  // Called when Core is destroyed.
  EXPECT_CALL(mock_controller, StopAllEndpoints)
      .WillOnce([&](ClientProxy* client, ResultCallback callback) {
        callback({Status::kSuccess});
      });
  EXPECT_CALL(mock_controller, RequestConnection);
  Core core{&mock_controller};
  core.RequestConnection("TEST", {}, {}, {});
}

TEST(CoreTest, RequestConnectionFailsWithEmptyEndpoint) {
  MockServiceControllerRouter mock_controller;
  // Called when Core is destroyed.
  EXPECT_CALL(mock_controller, StopAllEndpoints)
      .WillOnce([&](ClientProxy* client, ResultCallback callback) {
        callback({Status::kSuccess});
      });
  Status final_status;
  Core core{&mock_controller};
  core.RequestConnection(
      "", {}, {}, [&](Status result_status) { final_status = result_status; });
  EXPECT_EQ(final_status.value, Status::kEndpointUnknown);
}

TEST(CoreTest, AcceptConnectionCallsScRouter) {
  MockServiceControllerRouter mock_controller;
  // Called when Core is destroyed.
  EXPECT_CALL(mock_controller, StopAllEndpoints)
      .WillOnce([&](ClientProxy* client, ResultCallback callback) {
        callback({Status::kSuccess});
      });
  EXPECT_CALL(mock_controller, AcceptConnection);
  Core core{&mock_controller};
  core.AcceptConnection("TEST", {}, {});
}

TEST(CoreTest, AcceptConnectionFailsWithEmptyEndpoint) {
  MockServiceControllerRouter mock_controller;
  // Called when Core is destroyed.
  EXPECT_CALL(mock_controller, StopAllEndpoints)
      .WillOnce([&](ClientProxy* client, ResultCallback callback) {
        callback({Status::kSuccess});
      });
  Status final_status;
  Core core{&mock_controller};
  core.AcceptConnection(
      "", {}, [&](Status result_status) { final_status = result_status; });

  EXPECT_EQ(final_status.value, Status::kEndpointUnknown);
}

TEST(CoreTest, RejectConnectionFailsWithEmptyEndpoint) {
  MockServiceControllerRouter mock_controller;
  // Called when Core is destroyed.
  EXPECT_CALL(mock_controller, StopAllEndpoints)
      .WillOnce([&](ClientProxy* client, ResultCallback callback) {
        callback({Status::kSuccess});
      });
  Status final_status;
  Core core{&mock_controller};
  core.RejectConnection(
      "", [&](Status result_status) { final_status = result_status; });

  EXPECT_EQ(final_status.value, Status::kEndpointUnknown);
}

TEST(CoreTest, DisconnectFailsWithEmptyEndpoint) {
  MockServiceControllerRouter mock_controller;
  // Called when Core is destroyed.
  EXPECT_CALL(mock_controller, StopAllEndpoints)
      .WillOnce([&](ClientProxy* client, ResultCallback callback) {
        callback({Status::kSuccess});
      });
  Status final_status;
  Core core{&mock_controller};
  core.DisconnectFromEndpoint(
      "", [&](Status result_status) { final_status = result_status; });

  EXPECT_EQ(final_status.value, Status::kEndpointUnknown);
}

TEST(CoreTest, SendPayloadCallsScRouter) {
  MockServiceControllerRouter mock_controller;
  // Called when Core is destroyed.
  EXPECT_CALL(mock_controller, StopAllEndpoints)
      .WillOnce([&](ClientProxy* client, ResultCallback callback) {
        callback({Status::kSuccess});
      });
  EXPECT_CALL(mock_controller, SendPayload);
  Core core{&mock_controller};
  core.SendPayload({"TEST"}, Payload(ByteArray("Hello world")), {});
}

TEST(CoreV3Test, TestAdvertisingOptionsConversionWorks) {
  MockServiceControllerRouter mock_controller;
  // Called when Core is destroyed.
  EXPECT_CALL(mock_controller, StopAllEndpoints)
      .WillOnce([&](ClientProxy* client, ResultCallback callback) {
        callback({Status::kSuccess});
      });
  EXPECT_CALL(mock_controller, StartAdvertising)
      .WillOnce([](ClientProxy*, absl::string_view,
                   const AdvertisingOptions& options,
                   const ConnectionRequestInfo& info, ResultCallback) {
        EXPECT_EQ(options.strategy, Strategy::kP2pCluster);
        EXPECT_FALSE(options.low_power);
        EXPECT_TRUE(options.enable_bluetooth_listening);
        EXPECT_FALSE(options.auto_upgrade_bandwidth);
        EXPECT_EQ(options.fast_advertisement_service_uuid, "NearbyConnections");
      });
  Core core{&mock_controller};
  v3::AdvertisingOptions advertising_options = {
      .strategy = Strategy::kP2pCluster,
      .power_level = PowerLevel::kHighPower,
      .enable_bluetooth_listening = true,
      .auto_upgrade_bandwidth = false,
      .fast_advertisement_service_uuid = "NearbyConnections",
  };
  core.StartAdvertisingV3("service", advertising_options, {}, {});
  core.StopDiscoveryV3({});
}

TEST(CoreV3Test, TestDiscoveryOptionsConversionWorks) {
  MockServiceControllerRouter mock_controller;
  // Called when Core is destroyed.
  EXPECT_CALL(mock_controller, StopAllEndpoints)
      .WillOnce([&](ClientProxy* client, ResultCallback callback) {
        callback({Status::kSuccess});
      });
  EXPECT_CALL(mock_controller, StartDiscovery)
      .WillOnce([](ClientProxy*, absl::string_view,
                   const DiscoveryOptions& options, DiscoveryListener,
                   ResultCallback) {
        EXPECT_EQ(options.strategy, Strategy::kP2pCluster);
        EXPECT_FALSE(options.low_power);
        EXPECT_TRUE(options.auto_upgrade_bandwidth);
        EXPECT_EQ(options.fast_advertisement_service_uuid, "NearbyConnections");
      });
  Core core{&mock_controller};
  v3::DiscoveryOptions discovery_options = {
      .strategy = Strategy::kP2pCluster,
      .power_level = PowerLevel::kHighPower,
      .fast_advertisement_service_uuid = "NearbyConnections",
  };
  core.StartDiscoveryV3("service", discovery_options, {}, {});
  core.StopDiscoveryV3({});
}

TEST(CoreV3Test, TestCallbackWrapWorksStartAdvertisingV3FourArgs) {
  MockServiceControllerRouter mock_controller;
  EXPECT_CALL(mock_controller, StartAdvertising)
      .WillOnce([&](ClientProxy*, absl::string_view, const AdvertisingOptions&,
                    const ConnectionRequestInfo& info, const ResultCallback&) {
        LOG(INFO) << "StartAdvertising called";
        ASSERT_FALSE(info.endpoint_info.Empty());
        // call all callbacks to make sure it all gets called correctly.
        info.listener.initiated_cb("FAKE", {});
        info.listener.rejected_cb("FAKE", {Status::kConnectionRejected});
        info.listener.accepted_cb("FAKE");
        info.listener.bandwidth_changed_cb("FAKE", Medium::BLUETOOTH);
        info.listener.disconnected_cb("FAKE");
      });
  EXPECT_CALL(mock_controller, StopAllEndpoints)
      .WillOnce([&](ClientProxy* client, ResultCallback callback) {
        LOG(INFO) << "StopAllEndpoints called";
        callback({Status::kSuccess});
      });
  Core core{&mock_controller};
  absl::BlockingCounter result_latch(2);
  absl::Notification bandwidth_changed_latch;
  absl::Notification disconnected_latch;
  absl::Notification initiated_latch;
  v3::AdvertisingOptions advertising_options;
  advertising_options.strategy = Strategy::kP2pCluster;
  core.StartAdvertisingV3(
      "service", advertising_options,
      {
          .initiated_cb =
              [&initiated_latch](const NearbyDevice&,
                                 const v3::InitialConnectionInfo&) {
                initiated_latch.Notify();
              },
          .result_cb =
              [&result_latch](const NearbyDevice&,
                              v3::ConnectionResult resolution) {
                result_latch.DecrementCount();
              },
          .disconnected_cb =
              [&disconnected_latch](const NearbyDevice&) {
                disconnected_latch.Notify();
              },
          .bandwidth_changed_cb =
              [&bandwidth_changed_latch](const NearbyDevice&,
                                         v3::BandwidthInfo) {
                bandwidth_changed_latch.Notify();
              },
      },
      {});
  result_latch.Wait();
  bandwidth_changed_latch.WaitForNotification();
  disconnected_latch.WaitForNotification();
  initiated_latch.WaitForNotification();
  core.StopAdvertisingV3({});
}

TEST(CoreV3Test, TestStartAdvertisingV3NonConnectionsDeviceProvider) {
  MockServiceControllerRouter mock_controller;
  EXPECT_CALL(mock_controller, StartAdvertising)
      .WillOnce([&](ClientProxy*, absl::string_view, const AdvertisingOptions&,
                    const ConnectionRequestInfo& info, ResultCallback) {
        LOG(INFO) << "StartAdvertising called";
        ASSERT_TRUE(info.endpoint_info.Empty());
        // call all callbacks to make sure it all gets called correctly.
        info.listener.initiated_cb("FAKE", {});
        info.listener.rejected_cb("FAKE", {Status::kConnectionRejected});
        info.listener.accepted_cb("FAKE");
        info.listener.bandwidth_changed_cb("FAKE", Medium::BLUETOOTH);
        info.listener.disconnected_cb("FAKE");
      });
  EXPECT_CALL(mock_controller, StopAllEndpoints)
      .WillOnce([&](ClientProxy* client, ResultCallback callback) {
        LOG(INFO) << "StopAllEndpoints called";
        callback({Status::kSuccess});
      });
  Core core{&mock_controller};
  absl::BlockingCounter result_latch(2);
  absl::Notification bandwidth_changed_latch;
  absl::Notification disconnected_latch;
  absl::Notification initiated_latch;
  v3::AdvertisingOptions advertising_options;
  advertising_options.strategy = Strategy::kP2pCluster;
  FakeNearbyDeviceProvider device_provider;
  core.RegisterDeviceProvider(&device_provider);
  core.StartAdvertisingV3(
      "service", advertising_options,
      {
          .initiated_cb =
              [&initiated_latch](const NearbyDevice&,
                                 const v3::InitialConnectionInfo&) {
                initiated_latch.Notify();
              },
          .result_cb =
              [&result_latch](const NearbyDevice&,
                              v3::ConnectionResult resolution) {
                result_latch.DecrementCount();
              },
          .disconnected_cb =
              [&disconnected_latch](const NearbyDevice&) {
                disconnected_latch.Notify();
              },
          .bandwidth_changed_cb =
              [&bandwidth_changed_latch](const NearbyDevice&,
                                         v3::BandwidthInfo) {
                bandwidth_changed_latch.Notify();
              },
      },
      {});
  result_latch.Wait();
  bandwidth_changed_latch.WaitForNotification();
  disconnected_latch.WaitForNotification();
  initiated_latch.WaitForNotification();
  core.StopAdvertisingV3({});
}

TEST(CoreV3Test, TestStartAdvertisingV3NonConnectionsDevice) {
  MockServiceControllerRouter mock_controller;
  EXPECT_CALL(mock_controller, StartAdvertising)
      .WillOnce([&](ClientProxy*, absl::string_view, const AdvertisingOptions&,
                    const ConnectionRequestInfo& info, const ResultCallback&) {
        LOG(INFO) << "StartAdvertising called";
        ASSERT_TRUE(info.endpoint_info.Empty());
        // call all callbacks to make sure it all gets called correctly.
        info.listener.initiated_cb("FAKE", {});
        info.listener.rejected_cb("FAKE", {Status::kConnectionRejected});
        info.listener.accepted_cb("FAKE");
        info.listener.bandwidth_changed_cb("FAKE", Medium::BLUETOOTH);
        info.listener.disconnected_cb("FAKE");
      });
  EXPECT_CALL(mock_controller, StopAllEndpoints)
      .WillOnce([&](ClientProxy* client, ResultCallback callback) {
        LOG(INFO) << "StopAllEndpoints called";
        callback({Status::kSuccess});
      });
  Core core{&mock_controller};
  absl::BlockingCounter result_latch(2);
  absl::Notification bandwidth_changed_latch;
  absl::Notification disconnected_latch;
  absl::Notification initiated_latch;
  v3::AdvertisingOptions advertising_options;
  advertising_options.strategy = Strategy::kP2pCluster;
  auto local_device = FakeNearbyDevice();
  core.StartAdvertisingV3(
      "service", advertising_options, local_device,
      {
          .initiated_cb =
              [&initiated_latch](const NearbyDevice&,
                                 const v3::InitialConnectionInfo&) {
                initiated_latch.Notify();
              },
          .result_cb =
              [&result_latch](const NearbyDevice&,
                              v3::ConnectionResult resolution) {
                result_latch.DecrementCount();
              },
          .disconnected_cb =
              [&disconnected_latch](const NearbyDevice&) {
                disconnected_latch.Notify();
              },
          .bandwidth_changed_cb =
              [&bandwidth_changed_latch](const NearbyDevice&,
                                         v3::BandwidthInfo) {
                bandwidth_changed_latch.Notify();
              },
      },
      {});
  result_latch.Wait();
  bandwidth_changed_latch.WaitForNotification();
  disconnected_latch.WaitForNotification();
  initiated_latch.WaitForNotification();
  core.StopAdvertisingV3({});
}

TEST(CoreV3Test, TestCallbackWrapWorksStartAdvertisingV3FiveArgs) {
  MockServiceControllerRouter mock_controller;
  EXPECT_CALL(mock_controller, StartAdvertising)
      .WillOnce([&](ClientProxy*, absl::string_view, const AdvertisingOptions&,
                    const ConnectionRequestInfo& info, const ResultCallback&) {
        LOG(INFO) << "StartAdvertising called";
        ASSERT_FALSE(info.endpoint_info.Empty());
        // call all callbacks to make sure it all gets called correctly.
        info.listener.initiated_cb("FAKE", {});
        info.listener.rejected_cb("FAKE", {Status::kConnectionRejected});
        info.listener.accepted_cb("FAKE");
        info.listener.bandwidth_changed_cb("FAKE", Medium::BLUETOOTH);
        info.listener.disconnected_cb("FAKE");
      });
  EXPECT_CALL(mock_controller, StopAllEndpoints)
      .WillOnce([&](ClientProxy* client, ResultCallback callback) {
        LOG(INFO) << "StopAllEndpoints called";
        callback({Status::kSuccess});
      });
  Core core{&mock_controller};
  absl::BlockingCounter result_latch(2);
  absl::Notification bandwidth_changed_latch;
  absl::Notification disconnected_latch;
  absl::Notification initiated_latch;
  v3::AdvertisingOptions advertising_options;
  advertising_options.strategy = Strategy::kP2pCluster;
  auto local_device = v3::ConnectionsDevice("FAKE", "endpoint_info", {});
  core.StartAdvertisingV3(
      "service", advertising_options, local_device,
      {
          .initiated_cb =
              [&initiated_latch](const NearbyDevice&,
                                 const v3::InitialConnectionInfo&) {
                initiated_latch.Notify();
              },
          .result_cb =
              [&result_latch](const NearbyDevice&,
                              v3::ConnectionResult resolution) {
                result_latch.DecrementCount();
              },
          .disconnected_cb =
              [&disconnected_latch](const NearbyDevice&) {
                disconnected_latch.Notify();
              },
          .bandwidth_changed_cb =
              [&bandwidth_changed_latch](const NearbyDevice&,
                                         v3::BandwidthInfo) {
                bandwidth_changed_latch.Notify();
              },
      },
      {});
  result_latch.Wait();
  bandwidth_changed_latch.WaitForNotification();
  disconnected_latch.WaitForNotification();
  initiated_latch.WaitForNotification();
  core.StopAdvertisingV3({});
}

TEST(CoreV3Test, TestCallbackWrapWorksStartDiscoveryV3) {
  MockServiceControllerRouter mock_controller;
  EXPECT_CALL(mock_controller, StartDiscovery)
      .WillOnce([&](ClientProxy*, absl::string_view, const DiscoveryOptions&,
                    DiscoveryListener listener, const ResultCallback&) {
        // call all callbacks to make sure it all gets called correctly.
        LOG(INFO) << "StartDiscovery called";
        listener.endpoint_distance_changed_cb("FAKE", {});
        listener.endpoint_found_cb("FAKE", ByteArray(), "");
        listener.endpoint_lost_cb("FAKE");
      });
  EXPECT_CALL(mock_controller, StopAllEndpoints)
      .WillOnce([&](ClientProxy* client, ResultCallback callback) {
        LOG(INFO) << "StopAllEndpoints called";
        callback({Status::kSuccess});
      });
  v3::DiscoveryOptions options;
  options.strategy = Strategy::kP2pCluster;
  Core core{&mock_controller};
  absl::Notification endpoint_distance_latch;
  absl::Notification endpoint_found_latch;
  absl::Notification endpoint_lost_latch;
  core.StartDiscoveryV3(
      "service", options,
      {
          .endpoint_found_cb =
              [&endpoint_found_latch](const NearbyDevice&, absl::string_view) {
                endpoint_found_latch.Notify();
              },
          .endpoint_lost_cb =
              [&endpoint_lost_latch](const NearbyDevice&) {
                endpoint_lost_latch.Notify();
              },
          .endpoint_distance_changed_cb =
              [&endpoint_distance_latch](const NearbyDevice&, DistanceInfo) {
                endpoint_distance_latch.Notify();
              },
      },
      {});
  endpoint_distance_latch.WaitForNotification();
  endpoint_found_latch.WaitForNotification();
  endpoint_lost_latch.WaitForNotification();
  core.StopDiscoveryV3({});
}
}  // namespace
}  // namespace connections
}  // namespace nearby
