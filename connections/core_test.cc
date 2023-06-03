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

#include <memory>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "connections/implementation/mock_service_controller_router.h"
#include "connections/v3/bandwidth_info.h"
#include "connections/v3/connections_device.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace connections {
namespace {

class FakeNearbyDevice : public NearbyDevice {
 public:
  NearbyDevice::Type GetType() const override {
    return NearbyDevice::Type::kUnknownDevice;
  }
  MOCK_METHOD(std::string, GetEndpointId, (), (const override));
  MOCK_METHOD(std::vector<ConnectionInfoVariant>, GetConnectionInfos, (),
              (const override));
};

class FakeNearbyDeviceProvider : public NearbyDeviceProvider {
 public:
  const NearbyDevice* GetLocalDevice() override { return &local_device_; }

 private:
  FakeNearbyDevice local_device_;
};

TEST(CoreTest, ConstructorDestructorWorks) {
  MockServiceControllerRouter mock;
  // Called when Core is destroyed.
  EXPECT_CALL(mock, StopAllEndpoints)
      .WillOnce([&](ClientProxy* client, const ResultCallback& callback) {
        callback.result_cb({Status::kSuccess});
      });
  Core core{&mock};
}

TEST(CoreTest, DestructorReportsFatalFailure) {
  ASSERT_DEATH(
      {
        MockServiceControllerRouter mock;
        // Never invoke the result callback so ~Core will time out.
        EXPECT_CALL(mock, StopAllEndpoints);
        Core core{&mock};
      },
      "Unable to shutdown");
}

TEST(CoreV3Test, TestCallbackWrapWorksStartAdvertisingV3FourArgs) {
  MockServiceControllerRouter mock;
  EXPECT_CALL(mock, StartAdvertising)
      .WillOnce([&](ClientProxy*, absl::string_view, const AdvertisingOptions&,
                    const ConnectionRequestInfo& info, const ResultCallback&) {
        NEARBY_LOGS(INFO) << "StartAdvertising called";
        ASSERT_FALSE(info.endpoint_info.Empty());
        // call all callbacks to make sure it all gets called correctly.
        info.listener.initiated_cb("FAKE", {});
        info.listener.rejected_cb("FAKE", {Status::kConnectionRejected});
        info.listener.accepted_cb("FAKE");
        info.listener.bandwidth_changed_cb("FAKE", Medium::BLUETOOTH);
        info.listener.disconnected_cb("FAKE");
      });
  EXPECT_CALL(mock, StopAllEndpoints)
      .WillOnce([&](ClientProxy* client, const ResultCallback& callback) {
        NEARBY_LOGS(INFO) << "StopAllEndpoints called";
        callback.result_cb({Status::kSuccess});
      });
  Core core{&mock};
  CountDownLatch result_latch(2);
  CountDownLatch bandwidth_changed_latch(1);
  CountDownLatch disconnected_latch(1);
  CountDownLatch initiated_latch(1);
  AdvertisingOptions advertising_options;
  advertising_options.strategy = Strategy::kP2pCluster;
  core.StartAdvertisingV3(
      "service", advertising_options,
      {
          .initiated_cb =
              [&initiated_latch](const NearbyDevice&,
                                 const v3::InitialConnectionInfo&) {
                initiated_latch.CountDown();
              },
          .result_cb =
              [&result_latch](const NearbyDevice&,
                              v3::ConnectionResult resolution) {
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
              },
      },
      {});
  EXPECT_TRUE(result_latch.Await().Ok());
  EXPECT_TRUE(bandwidth_changed_latch.Await().Ok());
  EXPECT_TRUE(disconnected_latch.Await().Ok());
  EXPECT_TRUE(initiated_latch.Await().Ok());
  core.StopAdvertisingV3({});
}

TEST(CoreV3Test, TestStartAdvertisingV3NonConnectionsDeviceProvider) {
  MockServiceControllerRouter mock;
  EXPECT_CALL(mock, StartAdvertising)
      .WillOnce([&](ClientProxy*, absl::string_view, const AdvertisingOptions&,
                    const ConnectionRequestInfo& info, const ResultCallback&) {
        NEARBY_LOGS(INFO) << "StartAdvertising called";
        ASSERT_TRUE(info.endpoint_info.Empty());
        // call all callbacks to make sure it all gets called correctly.
        info.listener.initiated_cb("FAKE", {});
        info.listener.rejected_cb("FAKE", {Status::kConnectionRejected});
        info.listener.accepted_cb("FAKE");
        info.listener.bandwidth_changed_cb("FAKE", Medium::BLUETOOTH);
        info.listener.disconnected_cb("FAKE");
      });
  EXPECT_CALL(mock, StopAllEndpoints)
      .WillOnce([&](ClientProxy* client, const ResultCallback& callback) {
        NEARBY_LOGS(INFO) << "StopAllEndpoints called";
        callback.result_cb({Status::kSuccess});
      });
  Core core{&mock};
  CountDownLatch result_latch(2);
  CountDownLatch bandwidth_changed_latch(1);
  CountDownLatch disconnected_latch(1);
  CountDownLatch initiated_latch(1);
  AdvertisingOptions advertising_options;
  advertising_options.strategy = Strategy::kP2pCluster;
  core.RegisterDeviceProvider(std::make_unique<FakeNearbyDeviceProvider>());
  core.StartAdvertisingV3(
      "service", advertising_options,
      {
          .initiated_cb =
              [&initiated_latch](const NearbyDevice&,
                                 const v3::InitialConnectionInfo&) {
                initiated_latch.CountDown();
              },
          .result_cb =
              [&result_latch](const NearbyDevice&,
                              v3::ConnectionResult resolution) {
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
              },
      },
      {});
  EXPECT_TRUE(result_latch.Await().Ok());
  EXPECT_TRUE(bandwidth_changed_latch.Await().Ok());
  EXPECT_TRUE(disconnected_latch.Await().Ok());
  EXPECT_TRUE(initiated_latch.Await().Ok());
  core.StopAdvertisingV3({});
}

TEST(CoreV3Test, TestStartAdvertisingV3NonConnectionsDevice) {
  MockServiceControllerRouter mock;
  EXPECT_CALL(mock, StartAdvertising)
      .WillOnce([&](ClientProxy*, absl::string_view, const AdvertisingOptions&,
                    const ConnectionRequestInfo& info, const ResultCallback&) {
        NEARBY_LOGS(INFO) << "StartAdvertising called";
        ASSERT_TRUE(info.endpoint_info.Empty());
        // call all callbacks to make sure it all gets called correctly.
        info.listener.initiated_cb("FAKE", {});
        info.listener.rejected_cb("FAKE", {Status::kConnectionRejected});
        info.listener.accepted_cb("FAKE");
        info.listener.bandwidth_changed_cb("FAKE", Medium::BLUETOOTH);
        info.listener.disconnected_cb("FAKE");
      });
  EXPECT_CALL(mock, StopAllEndpoints)
      .WillOnce([&](ClientProxy* client, const ResultCallback& callback) {
        NEARBY_LOGS(INFO) << "StopAllEndpoints called";
        callback.result_cb({Status::kSuccess});
      });
  Core core{&mock};
  CountDownLatch result_latch(2);
  CountDownLatch bandwidth_changed_latch(1);
  CountDownLatch disconnected_latch(1);
  CountDownLatch initiated_latch(1);
  AdvertisingOptions advertising_options;
  advertising_options.strategy = Strategy::kP2pCluster;
  auto local_device = FakeNearbyDevice();
  core.StartAdvertisingV3(
      "service", advertising_options, local_device,
      {
          .initiated_cb =
              [&initiated_latch](const NearbyDevice&,
                                 const v3::InitialConnectionInfo&) {
                initiated_latch.CountDown();
              },
          .result_cb =
              [&result_latch](const NearbyDevice&,
                              v3::ConnectionResult resolution) {
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
              },
      },
      {});
  EXPECT_TRUE(result_latch.Await().Ok());
  EXPECT_TRUE(bandwidth_changed_latch.Await().Ok());
  EXPECT_TRUE(disconnected_latch.Await().Ok());
  EXPECT_TRUE(initiated_latch.Await().Ok());
  core.StopAdvertisingV3({});
}

TEST(CoreV3Test, TestCallbackWrapWorksStartAdvertisingV3FiveArgs) {
  MockServiceControllerRouter mock;
  EXPECT_CALL(mock, StartAdvertising)
      .WillOnce([&](ClientProxy*, absl::string_view, const AdvertisingOptions&,
                    const ConnectionRequestInfo& info, const ResultCallback&) {
        NEARBY_LOGS(INFO) << "StartAdvertising called";
        ASSERT_FALSE(info.endpoint_info.Empty());
        // call all callbacks to make sure it all gets called correctly.
        info.listener.initiated_cb("FAKE", {});
        info.listener.rejected_cb("FAKE", {Status::kConnectionRejected});
        info.listener.accepted_cb("FAKE");
        info.listener.bandwidth_changed_cb("FAKE", Medium::BLUETOOTH);
        info.listener.disconnected_cb("FAKE");
      });
  EXPECT_CALL(mock, StopAllEndpoints)
      .WillOnce([&](ClientProxy* client, const ResultCallback& callback) {
        NEARBY_LOGS(INFO) << "StopAllEndpoints called";
        callback.result_cb({Status::kSuccess});
      });
  Core core{&mock};
  CountDownLatch result_latch(2);
  CountDownLatch bandwidth_changed_latch(1);
  CountDownLatch disconnected_latch(1);
  CountDownLatch initiated_latch(1);
  AdvertisingOptions advertising_options;
  advertising_options.strategy = Strategy::kP2pCluster;
  auto local_device = v3::ConnectionsDevice("FAKE", "endpoint_info", {});
  core.StartAdvertisingV3(
      "service", advertising_options, local_device,
      {
          .initiated_cb =
              [&initiated_latch](const NearbyDevice&,
                                 const v3::InitialConnectionInfo&) {
                initiated_latch.CountDown();
              },
          .result_cb =
              [&result_latch](const NearbyDevice&,
                              v3::ConnectionResult resolution) {
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
              },
      },
      {});
  EXPECT_TRUE(result_latch.Await().Ok());
  EXPECT_TRUE(bandwidth_changed_latch.Await().Ok());
  EXPECT_TRUE(disconnected_latch.Await().Ok());
  EXPECT_TRUE(initiated_latch.Await().Ok());
  core.StopAdvertisingV3({});
}

TEST(CoreV3Test, TestCallbackWrapWorksStartDiscoveryV3) {
  MockServiceControllerRouter mock;
  EXPECT_CALL(mock, StartDiscovery)
      .WillOnce([&](ClientProxy*, absl::string_view, const DiscoveryOptions&,
                    const DiscoveryListener& info, const ResultCallback&) {
        // call all callbacks to make sure it all gets called correctly.
        NEARBY_LOGS(INFO) << "StartDiscovery called";
        info.endpoint_distance_changed_cb("FAKE", {});
        info.endpoint_found_cb("FAKE", ByteArray(), "");
        info.endpoint_lost_cb("FAKE");
      });
  EXPECT_CALL(mock, StopAllEndpoints)
      .WillOnce([&](ClientProxy* client, const ResultCallback& callback) {
        NEARBY_LOGS(INFO) << "StopAllEndpoints called";
        callback.result_cb({Status::kSuccess});
      });
  DiscoveryOptions options;
  options.strategy = Strategy::kP2pCluster;
  Core core{&mock};
  CountDownLatch endpoint_distance_latch(1);
  CountDownLatch endpoint_found_latch(1);
  CountDownLatch endpoint_lost_latch(1);
  core.StartDiscoveryV3(
      "service", options,
      {
          .endpoint_found_cb =
              [&endpoint_found_latch](const NearbyDevice&, absl::string_view) {
                endpoint_found_latch.CountDown();
              },
          .endpoint_lost_cb =
              [&endpoint_lost_latch](const NearbyDevice&) {
                endpoint_lost_latch.CountDown();
              },
          .endpoint_distance_changed_cb =
              [&endpoint_distance_latch](const NearbyDevice&, DistanceInfo) {
                endpoint_distance_latch.CountDown();
              },
      },
      {});
  EXPECT_TRUE(endpoint_distance_latch.Await().Ok());
  EXPECT_TRUE(endpoint_found_latch.Await().Ok());
  EXPECT_TRUE(endpoint_lost_latch.Await().Ok());
  core.StopDiscoveryV3({});
}
}  // namespace
}  // namespace connections
}  // namespace nearby
