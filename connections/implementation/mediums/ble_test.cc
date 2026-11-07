// Copyright 2022 Google LLC
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

#include "connections/implementation/mediums/ble.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "connections/implementation/flags/nearby_connections_feature_flags.h"
#include "connections/implementation/mediums/ble/ble_l2cap_packet.h"
#include "connections/implementation/mediums/ble/ble_socket.h"
#include "connections/implementation/mediums/ble/discovered_peripheral_callback.h"
#include "connections/implementation/mediums/bluetooth_radio.h"
#include "connections/implementation/pcp.h"
#include "connections/power_level.h"
#include "internal/flags/nearby_flags.h"
#include "internal/platform/ble.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/exception.h"
#include "internal/platform/expected.h"
#include "internal/platform/feature_flags.h"
#include "internal/platform/implementation/system_clock.h"
#include "internal/platform/logging.h"
#include "internal/platform/mac_address.h"
#include "internal/platform/medium_environment.h"
#include "internal/platform/single_thread_executor.h"

namespace nearby {
namespace connections {
namespace {

using FeatureFlags = FeatureFlags::Flags;
using location::nearby::proto::connections::OperationResultCode;

MATCHER_P(HasErrorResultCode, value, "") {
  return testing::ExplainMatchResult(testing::Optional(value),
                                     arg.error().operation_result_code(),
                                     result_listener);
}

constexpr FeatureFlags kTestCases[] = {
    FeatureFlags{
        .enable_cancellation_flag = true,
    },
    FeatureFlags{
        .enable_cancellation_flag = false,
    },
};

constexpr absl::Duration kWaitDuration = absl::Milliseconds(1000);
constexpr int64_t kPeripheralLostTimeoutInMillis = 1000;
constexpr char kServiceIDA[] = "com.google.location.nearby.apps.test.a";
constexpr char kServiceIDB[] = "com.google.location.nearby.apps.test.b";
constexpr absl::string_view kAdvertisementString = "\x0a\x0b\x0c\x0d";
constexpr absl::string_view kAdvertisementStringB = "\x01\x02\x03\x04";
constexpr char kLocalEndpointId[] = "local_endpoint_id";
constexpr char kFastAdvertisementServiceUuid[] = "\xf3\xfe";

class BleTest : public testing::TestWithParam<FeatureFlags> {
 public:
  void SetUp() override {
    NearbyFlags::GetInstance().OverrideInt64FlagValue(
        config_package_nearby::nearby_connections_feature::
            kBlePeripheralLostTimeoutMillis,
        /*ms=*/kPeripheralLostTimeoutInMillis);
  }

 protected:
  std::optional<BlePeripheral> StartAdvertisingAndDiscoverPeripheral(
      Ble& ble_server, Ble& ble_client, const std::string& service_id,
      const ByteArray& advertisement_bytes) {
    if (!ble_server
             .StartAdvertising(service_id, PowerLevel::kHighPower,
                               Ble::AdvertisingType::kFast, advertisement_bytes)
             .has_value()) {
      return std::nullopt;
    }

    absl::Notification discovered_notification;
    BlePeripheral discovered_peripheral;
    if (!ble_client
             .StartScanning(
                 service_id, Pcp::kP2pPointToPoint, PowerLevel::kHighPower,
                 /*include_dct_advertisement=*/false,
                 mediums::DiscoveredPeripheralCallback{
                     .peripheral_discovered_cb =
                         [&discovered_notification, &discovered_peripheral](
                             BlePeripheral peripheral,
                             const std::string& service_id,
                             const ByteArray& advertisement_bytes,
                             bool fast_advertisement) {
                           discovered_peripheral = peripheral;
                           if (!discovered_notification.HasBeenNotified()) {
                             discovered_notification.Notify();
                           }
                         },
                 })
             .has_value()) {
      return std::nullopt;
    }
    if (!discovered_notification.WaitForNotificationWithTimeout(
            kWaitDuration) ||
        !discovered_peripheral.IsValid()) {
      return std::nullopt;
    }
    return discovered_peripheral;
  }

  MediumEnvironment& env_{MediumEnvironment::Instance()};
};

TEST_P(BleTest, CanConnect) {
  FeatureFlags feature_flags = GetParam();
  env_.SetFeatureFlags(feature_flags);
  env_.Start();
  BluetoothRadio radio_client;
  BluetoothRadio radio_server;
  Ble ble_client{radio_client};
  Ble ble_server{radio_server};
  radio_client.Enable();
  radio_server.Enable();
  std::string service_id(kServiceIDA);
  auto advertisement_bytes = ByteArray::FromStringView(kAdvertisementString);
  CountDownLatch discovered_latch(1);
  CountDownLatch accept_latch(1);

  BleSocket socket_for_server;
  EXPECT_TRUE(ble_server.StartAcceptingConnections(
      service_id, [&](BleSocket socket, const std::string&) {
        socket_for_server = std::move(socket);
        accept_latch.CountDown();
      }));

  ble_server.StartAdvertising(service_id, PowerLevel::kHighPower,
                              Ble::AdvertisingType::kFast, advertisement_bytes);

  BlePeripheral discovered_peripheral;
  ble_client.StartScanning(
      service_id, Pcp::kP2pPointToPoint, PowerLevel::kHighPower,
      /*include_dct_advertisement=*/false,
      mediums::DiscoveredPeripheralCallback{
          .peripheral_discovered_cb =
              [&discovered_latch, &discovered_peripheral](
                  BlePeripheral peripheral, const std::string& service_id,
                  const ByteArray& advertisement_bytes,
                  bool fast_advertisement) {
                discovered_peripheral = peripheral;
                LOG(INFO) << "Discovered peripheral, fast advertisement="
                          << fast_advertisement;
                discovered_latch.CountDown();
              },
      });
  discovered_latch.Await(kWaitDuration).result();
  ASSERT_TRUE(discovered_peripheral.IsValid());

  CancellationFlag flag;
  ErrorOr<BleSocket> socket_for_client_result =
      ble_client.Connect(service_id, discovered_peripheral, &flag);
  EXPECT_TRUE(accept_latch.Await(kWaitDuration).result());
  EXPECT_TRUE(ble_server.StopAcceptingConnections(service_id));
  EXPECT_TRUE(ble_server.StopAdvertising(service_id));
  EXPECT_TRUE(socket_for_server.IsValid());
  EXPECT_TRUE(socket_for_client_result.has_value());
  EXPECT_TRUE(socket_for_client_result.value().IsValid());
  EXPECT_TRUE(socket_for_server.GetRemotePeripheral().IsValid());
  EXPECT_TRUE(socket_for_client_result.value().GetRemotePeripheral().IsValid());
  env_.Stop();
}

TEST_P(BleTest, CanConnect2) {
  FeatureFlags feature_flags = GetParam();
  env_.SetFeatureFlags(feature_flags);
  env_.Start();
  BluetoothRadio radio_client;
  BluetoothRadio radio_server;
  Ble ble_client{radio_client};
  Ble ble_server{radio_server};
  radio_client.Enable();
  radio_server.Enable();
  std::string service_id(kServiceIDA);
  auto advertisement_bytes = ByteArray::FromStringView(kAdvertisementString);
  CountDownLatch discovered_latch(1);
  CountDownLatch accept_latch(1);

  std::unique_ptr<mediums::BleSocket> socket_for_server;
  EXPECT_TRUE(ble_server.StartAcceptingConnections(
      service_id,
      [&](std::unique_ptr<mediums::BleSocket> socket, const std::string&) {
        socket_for_server = std::move(socket);
        accept_latch.CountDown();
      }));

  ble_server.StartAdvertising(service_id, PowerLevel::kHighPower,
                              Ble::AdvertisingType::kFast, advertisement_bytes);

  BlePeripheral discovered_peripheral;
  ble_client.StartScanning(
      service_id, Pcp::kP2pPointToPoint, PowerLevel::kHighPower,
      /*include_dct_advertisement=*/false,
      mediums::DiscoveredPeripheralCallback{
          .peripheral_discovered_cb =
              [&discovered_latch, &discovered_peripheral](
                  BlePeripheral peripheral, const std::string& service_id,
                  const ByteArray& advertisement_bytes,
                  bool fast_advertisement) {
                discovered_peripheral = peripheral;
                LOG(INFO) << "Discovered peripheral, fast advertisement="
                          << fast_advertisement;
                discovered_latch.CountDown();
              },
      });
  discovered_latch.Await(kWaitDuration).result();
  ASSERT_TRUE(discovered_peripheral.IsValid());

  CancellationFlag flag;
  ErrorOr<std::unique_ptr<mediums::BleSocket>> socket_for_client_result =
      ble_client.Connect2(service_id, discovered_peripheral, &flag);
  EXPECT_TRUE(accept_latch.Await(kWaitDuration).result());
  EXPECT_TRUE(ble_server.StopAcceptingConnections(service_id));
  EXPECT_TRUE(ble_server.StopAdvertising(service_id));
  ASSERT_NE(socket_for_server, nullptr);
  EXPECT_TRUE(socket_for_server.get()->IsValid());
  ASSERT_TRUE(socket_for_client_result.has_value());
  EXPECT_TRUE(socket_for_client_result.value()->IsValid());
  EXPECT_TRUE(socket_for_server->GetRemotePeripheral().IsValid());
  EXPECT_TRUE(
      socket_for_client_result.value()->GetRemotePeripheral().IsValid());
  env_.Stop();
}

TEST_P(BleTest, CanCancelConnect) {
  FeatureFlags feature_flags = GetParam();
  env_.SetFeatureFlags(feature_flags);
  env_.Start();
  BluetoothRadio radio_client;
  BluetoothRadio radio_server;
  Ble ble_client{radio_client};
  Ble ble_server{radio_server};
  radio_client.Enable();
  radio_server.Enable();
  std::string service_id(kServiceIDA);
  auto advertisement_bytes = ByteArray::FromStringView(kAdvertisementString);
  CountDownLatch discovered_latch(1);
  CountDownLatch accept_latch(1);

  BleSocket socket_for_server;
  EXPECT_TRUE(ble_server.StartAcceptingConnections(
      service_id, [&](BleSocket socket, const std::string&) {
        socket_for_server = std::move(socket);
        accept_latch.CountDown();
      }));

  ble_server.StartAdvertising(service_id, PowerLevel::kHighPower,
                              Ble::AdvertisingType::kFast, advertisement_bytes);

  BlePeripheral discovered_peripheral;
  ble_client.StartScanning(
      service_id, Pcp::kP2pPointToPoint, PowerLevel::kHighPower,
      /*include_dct_advertisement=*/false,
      mediums::DiscoveredPeripheralCallback{
          .peripheral_discovered_cb =
              [&discovered_latch, &discovered_peripheral](
                  BlePeripheral peripheral, const std::string& service_id,
                  const ByteArray& advertisement_bytes,
                  bool fast_advertisement) {
                discovered_peripheral = peripheral;
                LOG(INFO) << "Discovered peripheral, fast advertisement="
                          << fast_advertisement;
                discovered_latch.CountDown();
              },
      });
  EXPECT_TRUE(discovered_latch.Await(kWaitDuration).result());
  ASSERT_TRUE(discovered_peripheral.IsValid());

  CancellationFlag flag(true);
  ErrorOr<BleSocket> socket_for_client_result =
      ble_client.Connect(service_id, discovered_peripheral, &flag);
  // If FeatureFlag is disabled, Cancelled is false as no-op.
  if (!feature_flags.enable_cancellation_flag) {
    EXPECT_TRUE(accept_latch.Await(kWaitDuration).result());
    EXPECT_TRUE(ble_server.StopAcceptingConnections(service_id));
    EXPECT_TRUE(ble_server.StopAdvertising(service_id));
    EXPECT_TRUE(socket_for_server.IsValid());
    EXPECT_TRUE(socket_for_client_result.has_value());
    EXPECT_TRUE(socket_for_client_result.value().IsValid());
    EXPECT_TRUE(socket_for_server.GetRemotePeripheral().IsValid());
    EXPECT_TRUE(
        socket_for_client_result.value().GetRemotePeripheral().IsValid());
  } else {
    EXPECT_FALSE(accept_latch.Await(kWaitDuration).result());
    EXPECT_TRUE(ble_server.StopAcceptingConnections(service_id));
    EXPECT_TRUE(ble_server.StopAdvertising(service_id));
    EXPECT_FALSE(socket_for_server.IsValid());
    EXPECT_TRUE(socket_for_client_result.has_error());
  }
  env_.Stop();
}

TEST_P(BleTest, CanCancelConnect2) {
  FeatureFlags feature_flags = GetParam();
  env_.SetFeatureFlags(feature_flags);
  env_.Start();
  BluetoothRadio radio_client;
  BluetoothRadio radio_server;
  Ble ble_client{radio_client};
  Ble ble_server{radio_server};
  radio_client.Enable();
  radio_server.Enable();
  std::string service_id(kServiceIDA);
  auto advertisement_bytes = ByteArray::FromStringView(kAdvertisementString);
  CountDownLatch discovered_latch(1);
  CountDownLatch accept_latch(1);

  std::unique_ptr<mediums::BleSocket> socket_for_server;
  EXPECT_TRUE(ble_server.StartAcceptingConnections(
      service_id,
      [&](std::unique_ptr<mediums::BleSocket> socket, const std::string&) {
        socket_for_server = std::move(socket);
        accept_latch.CountDown();
      }));

  ble_server.StartAdvertising(service_id, PowerLevel::kHighPower,
                              Ble::AdvertisingType::kFast, advertisement_bytes);

  BlePeripheral discovered_peripheral;
  ble_client.StartScanning(
      service_id, Pcp::kP2pPointToPoint, PowerLevel::kHighPower,
      /*include_dct_advertisement=*/false,
      mediums::DiscoveredPeripheralCallback{
          .peripheral_discovered_cb =
              [&discovered_latch, &discovered_peripheral](
                  BlePeripheral peripheral, const std::string& service_id,
                  const ByteArray& advertisement_bytes,
                  bool fast_advertisement) {
                discovered_peripheral = peripheral;
                LOG(INFO) << "Discovered peripheral, fast advertisement="
                          << fast_advertisement;
                discovered_latch.CountDown();
              },
      });
  EXPECT_TRUE(discovered_latch.Await(kWaitDuration).result());
  ASSERT_TRUE(discovered_peripheral.IsValid());

  CancellationFlag flag(true);
  ErrorOr<std::unique_ptr<mediums::BleSocket>> socket_for_client_result =
      ble_client.Connect2(service_id, discovered_peripheral, &flag);
  // If FeatureFlag is disabled, Cancelled is false as no-op.
  if (!feature_flags.enable_cancellation_flag) {
    EXPECT_TRUE(accept_latch.Await(kWaitDuration).result());
    EXPECT_TRUE(ble_server.StopAcceptingConnections(service_id));
    EXPECT_TRUE(ble_server.StopAdvertising(service_id));
    ASSERT_NE(socket_for_server, nullptr);
    EXPECT_TRUE(socket_for_server.get()->IsValid());
    EXPECT_TRUE(socket_for_client_result.has_value());
    EXPECT_TRUE(socket_for_server.get()->GetRemotePeripheral().IsValid());
    EXPECT_TRUE(
        socket_for_client_result.value()->GetRemotePeripheral().IsValid());
  } else {
    EXPECT_FALSE(accept_latch.Await(kWaitDuration).result());
    EXPECT_TRUE(ble_server.StopAcceptingConnections(service_id));
    EXPECT_TRUE(ble_server.StopAdvertising(service_id));
    ASSERT_EQ(socket_for_server, nullptr);
    EXPECT_TRUE(socket_for_client_result.has_error());
  }
  env_.Stop();
}

// TODO: b/419654808 - Enable this test when L2CAP connection refactor is fully
// supported.
TEST_P(BleTest, DISABLED_CanConnectOverL2capRefactored) {
  FeatureFlags feature_flags = GetParam();
  env_.SetFeatureFlags(feature_flags);
  env_.Start();
  BluetoothRadio radio_client;
  BluetoothRadio radio_server;
  Ble ble_client{radio_client};
  Ble ble_server{radio_server};
  radio_client.Enable();
  radio_server.Enable();
  std::string service_id(kServiceIDA);
  auto advertisement_bytes = ByteArray::FromStringView(kAdvertisementString);
  CountDownLatch discovered_latch(1);
  CountDownLatch accept_latch(1);

  std::unique_ptr<mediums::BleSocket> socket_for_server;
  auto result = ble_server.StartAcceptingL2capConnections(
      service_id,
      [&](std::unique_ptr<mediums::BleSocket> socket, const std::string&) {
        socket_for_server = std::move(socket);
        accept_latch.CountDown();
      });
  if (!result.has_value()) {
    GTEST_SKIP() << "L2CAP not supported";
  }

  ble_server.StartAdvertising(service_id, PowerLevel::kHighPower,
                              Ble::AdvertisingType::kFast, advertisement_bytes);

  BlePeripheral discovered_peripheral;
  ble_client.StartScanning(
      service_id, Pcp::kP2pPointToPoint, PowerLevel::kHighPower,
      /*include_dct_advertisement=*/false,
      mediums::DiscoveredPeripheralCallback{
          .peripheral_discovered_cb =
              [&discovered_latch, &discovered_peripheral](
                  BlePeripheral peripheral, const std::string& service_id,
                  const ByteArray& advertisement_bytes,
                  bool fast_advertisement) {
                discovered_peripheral = peripheral;
                LOG(INFO) << "Discovered peripheral, fast advertisement="
                          << fast_advertisement;
                discovered_latch.CountDown();
              },
      });
  discovered_latch.Await(kWaitDuration);
  ASSERT_TRUE(discovered_peripheral.IsValid());

  CancellationFlag flag;
  ErrorOr<std::unique_ptr<mediums::BleSocket>> socket_for_client_result =
      ble_client.ConnectOverL2cap2(service_id, discovered_peripheral, &flag);
  EXPECT_TRUE(accept_latch.Await(kWaitDuration).result());
  EXPECT_TRUE(ble_server.StopAcceptingL2capConnections(service_id));
  EXPECT_TRUE(ble_server.StopAdvertising(service_id));
  ASSERT_NE(socket_for_server, nullptr);
  EXPECT_TRUE(socket_for_server->IsValid());
  EXPECT_TRUE(socket_for_client_result.has_value());
  EXPECT_NE(socket_for_client_result.value(), nullptr);
  EXPECT_TRUE(socket_for_client_result.value()->IsValid());
  EXPECT_TRUE(socket_for_server->GetRemotePeripheral().IsValid());
  EXPECT_TRUE(
      socket_for_client_result.value()->GetRemotePeripheral().IsValid());
  env_.Stop();
}

INSTANTIATE_TEST_SUITE_P(ParametrisedBleTest, BleTest,
                         ::testing::ValuesIn(kTestCases));

TEST_F(BleTest, CanConstructValidObject) {
  env_.Start();
  BluetoothRadio radio_a;
  BluetoothRadio radio_b;
  Ble ble_a(radio_a);
  Ble ble_b(radio_b);

  EXPECT_TRUE(ble_a.IsMediumValid());
  EXPECT_TRUE(ble_a.IsAvailable());
  EXPECT_TRUE(ble_b.IsMediumValid());
  EXPECT_TRUE(ble_b.IsAvailable());
  EXPECT_NE(&radio_a.GetBluetoothAdapter(), &radio_b.GetBluetoothAdapter());
  env_.Stop();
}

TEST_F(BleTest, CanNotStartAdvertisingWithEmptyAdvertisementBytes) {
  env_.Start();
  BluetoothRadio radio_a;
  Ble ble_a(radio_a);
  radio_a.Enable();
  ByteArray advertisement_bytes;

  EXPECT_FALSE(ble_a.StartAdvertising(kServiceIDA, PowerLevel::kHighPower,
                                      Ble::AdvertisingType::kRegular,
                                      advertisement_bytes));
  env_.Stop();
}

TEST_F(BleTest, CanNotStartAdvertisingWhenRadioNotEnabled) {
  env_.Start();
  BluetoothRadio radio_a;
  Ble ble_a(radio_a);
  radio_a.Disable();
  auto advertisement_bytes = ByteArray::FromStringView(kAdvertisementString);

  EXPECT_FALSE(ble_a.StartAdvertising(kServiceIDA, PowerLevel::kHighPower,
                                      Ble::AdvertisingType::kRegular,
                                      advertisement_bytes));
  env_.Stop();
}

TEST_F(BleTest, CanStartAdvertisingWithDifferentPowerLevels) {
  env_.Start();
  BluetoothRadio radio_a;
  Ble ble_a(radio_a);
  radio_a.Enable();
  auto advertisement_bytes = ByteArray::FromStringView(kAdvertisementString);

  EXPECT_TRUE(ble_a.StartAdvertising(kServiceIDA, PowerLevel::kLowPower,
                                     Ble::AdvertisingType::kRegular,
                                     advertisement_bytes));
  EXPECT_TRUE(ble_a.StopAdvertising(kServiceIDA));
  EXPECT_TRUE(ble_a.StartAdvertising(kServiceIDA, PowerLevel::kHighPower,
                                     Ble::AdvertisingType::kRegular,
                                     advertisement_bytes));
  EXPECT_TRUE(ble_a.StopAdvertising(kServiceIDA));
  env_.Stop();
}

TEST_F(BleTest, CanStartAdvertisingWithDifferentAdvertisingTypes) {
  env_.Start();
  BluetoothRadio radio_a;
  Ble ble_a(radio_a);
  radio_a.Enable();
  auto advertisement_bytes = ByteArray::FromStringView(kAdvertisementString);

  EXPECT_TRUE(ble_a.StartAdvertising(kServiceIDA, PowerLevel::kHighPower,
                                     Ble::AdvertisingType::kRegular,
                                     advertisement_bytes));
  EXPECT_TRUE(ble_a.StopAdvertising(kServiceIDA));
  EXPECT_TRUE(ble_a.StartAdvertising(kServiceIDA, PowerLevel::kHighPower,
                                     Ble::AdvertisingType::kFast,
                                     advertisement_bytes));
  EXPECT_TRUE(ble_a.StopAdvertising(kServiceIDA));
  env_.Stop();
}

TEST_F(BleTest, CanStartFastAdvertising) {
  env_.Start();
  BluetoothRadio radio_a;
  BluetoothRadio radio_b;
  Ble ble_a(radio_a);
  Ble ble_b(radio_b);
  radio_a.Enable();
  radio_b.Enable();
  auto advertisement_bytes = ByteArray::FromStringView(kAdvertisementString);
  CountDownLatch found_latch(1);

  ble_b.StartScanning(
      kServiceIDA, Pcp::kP2pPointToPoint, PowerLevel::kHighPower,
      /*include_dct_advertisement=*/false,
      mediums::DiscoveredPeripheralCallback{
          .peripheral_discovered_cb =
              [&found_latch](BlePeripheral peripheral,
                             const std::string& service_id,
                             const ByteArray& advertisement_bytes,
                             bool fast_advertisement) {
                EXPECT_TRUE(fast_advertisement);
                found_latch.CountDown();
              },
      });

  EXPECT_TRUE(ble_a.StartAdvertising(kServiceIDA, PowerLevel::kHighPower,
                                     Ble::AdvertisingType::kFast,
                                     advertisement_bytes));
  EXPECT_TRUE(found_latch.Await(kWaitDuration).result());
  EXPECT_TRUE(ble_a.StopAdvertising(kServiceIDA));
  ble_b.StopScanning(kServiceIDA);
  env_.Stop();
}

TEST_F(BleTest, CanStartFastScanning) {
  env_.Start();
  BluetoothRadio radio_a;
  BluetoothRadio radio_b;
  Ble ble_a(radio_a);
  Ble ble_b(radio_b);
  radio_a.Enable();
  radio_b.Enable();
  auto advertisement_bytes = ByteArray::FromStringView(kAdvertisementString);
  CountDownLatch found_latch(1);

  ble_b.StartAdvertising(kServiceIDA, PowerLevel::kHighPower,
                         Ble::AdvertisingType::kFast, advertisement_bytes);

  EXPECT_TRUE(ble_a.StartScanning(
      kServiceIDA, Pcp::kP2pPointToPoint, PowerLevel::kHighPower,
      /*include_dct_advertisement=*/false,
      mediums::DiscoveredPeripheralCallback{
          .peripheral_discovered_cb =
              [&found_latch](BlePeripheral peripheral,
                             const std::string& service_id,
                             const ByteArray& advertisement_bytes,
                             bool fast_advertisement) {
                EXPECT_TRUE(fast_advertisement);
                found_latch.CountDown();
              },
      }));

  EXPECT_TRUE(found_latch.Await(kWaitDuration).result());
  ble_b.StopAdvertising(kServiceIDA);
  EXPECT_TRUE(ble_a.StopScanning(kServiceIDA));
  env_.Stop();
}

TEST_F(BleTest, CanStartAdvertising) {
  env_.Start();
  BluetoothRadio radio_a;
  BluetoothRadio radio_b;
  Ble ble_a(radio_a);
  Ble ble_b(radio_b);
  radio_a.Enable();
  radio_b.Enable();
  auto advertisement_bytes = ByteArray::FromStringView(kAdvertisementString);
  CountDownLatch found_latch(1);

  ble_b.StartScanning(
      kServiceIDA, Pcp::kP2pPointToPoint, PowerLevel::kHighPower,
      /*include_dct_advertisement=*/false,
      mediums::DiscoveredPeripheralCallback{
          .peripheral_discovered_cb =
              [&found_latch](BlePeripheral peripheral,
                             const std::string& service_id,
                             const ByteArray& advertisement_bytes,
                             bool fast_advertisement) {
                EXPECT_FALSE(fast_advertisement);
                found_latch.CountDown();
              },
      });

  EXPECT_TRUE(ble_a.StartAdvertising(kServiceIDA, PowerLevel::kHighPower,
                                     Ble::AdvertisingType::kRegular,
                                     advertisement_bytes));
  EXPECT_TRUE(found_latch.Await(kWaitDuration).result());
  EXPECT_TRUE(ble_a.StopAdvertising(kServiceIDA));
  ble_b.StopScanning(kServiceIDA);
  env_.Stop();
}

TEST_F(BleTest, CanStartScanning) {
  env_.Start();
  BluetoothRadio radio_a;
  BluetoothRadio radio_b;
  Ble ble_a(radio_a);
  Ble ble_b(radio_b);
  radio_a.Enable();
  radio_b.Enable();
  auto advertisement_bytes = ByteArray::FromStringView(kAdvertisementString);
  CountDownLatch found_latch(1);

  ble_b.StartAdvertising(kServiceIDA, PowerLevel::kHighPower,
                         Ble::AdvertisingType::kRegular, advertisement_bytes);

  EXPECT_TRUE(ble_a.StartScanning(
      kServiceIDA, Pcp::kP2pPointToPoint, PowerLevel::kHighPower,
      /*include_dct_advertisement=*/false,
      mediums::DiscoveredPeripheralCallback{
          .peripheral_discovered_cb =
              [&found_latch](BlePeripheral peripheral,
                             const std::string& service_id,
                             const ByteArray& advertisement_bytes,
                             bool fast_advertisement) {
                EXPECT_FALSE(fast_advertisement);
                found_latch.CountDown();
              },
      }));

  EXPECT_TRUE(found_latch.Await(kWaitDuration).result());
  ble_b.StopAdvertising(kServiceIDA);
  EXPECT_TRUE(ble_a.StopScanning(kServiceIDA));
  env_.Stop();
}

TEST_F(BleTest, CanNotStartScanningWhenRadioNotEnabled) {
  env_.Start();
  BluetoothRadio radio_a;
  Ble ble_a(radio_a);
  radio_a.Disable();

  EXPECT_FALSE(ble_a.StartScanning(kServiceIDA, Pcp::kP2pPointToPoint,
                                   PowerLevel::kHighPower,
                                   /*include_dct_advertisement=*/false,
                                   mediums::DiscoveredPeripheralCallback{}));
  env_.Stop();
}

TEST_F(BleTest, CanNotStartAcceptingConnectionsWhenRadioNotEnabled) {
  env_.Start();
  BluetoothRadio radio_a;
  Ble ble_a(radio_a);
  radio_a.Disable();

  EXPECT_THAT(ble_a.StartAcceptingConnections(
                  kServiceIDA, Ble::AcceptedConnectionCallback()),
              HasErrorResultCode(
                  OperationResultCode::MISCELLEANEOUS_BT_SYSTEM_SERVICE_NULL));
  EXPECT_THAT(ble_a.StartAcceptingConnections(
                  kServiceIDA, Ble::AcceptedConnectionCallback2()),
              HasErrorResultCode(
                  OperationResultCode::MISCELLEANEOUS_BT_SYSTEM_SERVICE_NULL));
  env_.Stop();
}

TEST_F(BleTest, CanNotStartAcceptingConnectionsWithEmptyServiceId) {
  env_.Start();
  BluetoothRadio radio_a;
  Ble ble_a(radio_a);
  radio_a.Enable();

  EXPECT_THAT(
      ble_a.StartAcceptingConnections("", Ble::AcceptedConnectionCallback()),
      HasErrorResultCode(
          OperationResultCode::CLIENT_BLE_DUPLICATE_DISCOVERING));
  EXPECT_THAT(
      ble_a.StartAcceptingConnections("", Ble::AcceptedConnectionCallback2()),
      HasErrorResultCode(
          OperationResultCode::CLIENT_BLE_DUPLICATE_DISCOVERING));
  env_.Stop();
}

TEST_F(BleTest, CanStartScanningWithDifferentPowerLevels) {
  env_.Start();
  BluetoothRadio radio_a;
  Ble ble_a(radio_a);
  radio_a.Enable();

  EXPECT_TRUE(ble_a.StartScanning(kServiceIDA, Pcp::kP2pPointToPoint,
                                  PowerLevel::kLowPower,
                                  /*include_dct_advertisement=*/false,
                                  mediums::DiscoveredPeripheralCallback{}));
  EXPECT_TRUE(ble_a.StopScanning(kServiceIDA));
  EXPECT_TRUE(ble_a.StartScanning(kServiceIDA, Pcp::kP2pPointToPoint,
                                  PowerLevel::kHighPower,
                                  /*include_dct_advertisement=*/false,
                                  mediums::DiscoveredPeripheralCallback{}));
  EXPECT_TRUE(ble_a.StopScanning(kServiceIDA));
  env_.Stop();
}

TEST_F(BleTest, CanNotConnectWithInvalidPeripheral) {
  env_.Start();
  BluetoothRadio radio_client;
  Ble ble_client{radio_client};
  radio_client.Enable();
  std::string service_id(kServiceIDA);
  BlePeripheral invalid_peripheral;
  CancellationFlag flag;

  ErrorOr<BleSocket> socket_for_client_result =
      ble_client.Connect(service_id, invalid_peripheral, &flag);
  EXPECT_TRUE(socket_for_client_result.has_error());
  env_.Stop();
}

TEST_F(BleTest, CanNotConnectWithEmptyServiceId) {
  env_.Start();
  BluetoothRadio radio_client;
  Ble ble_client{radio_client};
  radio_client.Enable();
  std::string empty_service_id;
  BlePeripheral peripheral;
  CancellationFlag flag;

  ErrorOr<BleSocket> socket_for_client_result =
      ble_client.Connect(empty_service_id, peripheral, &flag);
  EXPECT_TRUE(socket_for_client_result.has_error());
  env_.Stop();
}

TEST_F(BleTest, CanStartStopMultipleScanningWithDifferentServiceIds) {
  env_.Start();
  BluetoothRadio radio;
  Ble ble(radio);
  radio.Enable();

  EXPECT_TRUE(ble.StartScanning(kServiceIDA, Pcp::kP2pPointToPoint,
                                PowerLevel::kHighPower,
                                /*include_dct_advertisement=*/false,
                                mediums::DiscoveredPeripheralCallback{}));
  EXPECT_TRUE(ble.StartScanning(kServiceIDB, Pcp::kP2pPointToPoint,
                                PowerLevel::kHighPower,
                                /*include_dct_advertisement=*/false,
                                mediums::DiscoveredPeripheralCallback{}));
  EXPECT_TRUE(ble.StopScanning(kServiceIDA));

  EXPECT_TRUE(ble.StartScanning(kServiceIDA, Pcp::kP2pPointToPoint,
                                PowerLevel::kHighPower,
                                /*include_dct_advertisement=*/false,
                                mediums::DiscoveredPeripheralCallback{}));
  EXPECT_TRUE(ble.StopScanning(kServiceIDA));
  EXPECT_TRUE(ble.StopScanning(kServiceIDB));
  env_.Stop();
}

TEST_F(BleTest, DestructWorksForStartAdvertisingAndScanningWithoutStop) {
  env_.Start();
  BluetoothRadio radio_a;
  BluetoothRadio radio_b;
  Ble ble_a(radio_a);
  Ble ble_b(radio_b);

  radio_a.Enable();
  radio_b.Enable();
  auto advertisement_bytes = ByteArray::FromStringView(kAdvertisementString);

  // Device A starts advertising with service IDA and IDB.
  EXPECT_TRUE(ble_a.StartAdvertising(kServiceIDA, PowerLevel::kHighPower,
                                     Ble::AdvertisingType::kFast,
                                     advertisement_bytes));
  EXPECT_TRUE(ble_a.StartAdvertising(kServiceIDB, PowerLevel::kHighPower,
                                     Ble::AdvertisingType::kFast,
                                     advertisement_bytes));

  // Device B starts scanning with service IDA and IDB
  EXPECT_TRUE(ble_b.StartScanning(kServiceIDA, Pcp::kP2pPointToPoint,
                                  PowerLevel::kHighPower,
                                  /*include_dct_advertisement=*/false,
                                  mediums::DiscoveredPeripheralCallback{}));
  EXPECT_TRUE(ble_b.StartScanning(kServiceIDB, Pcp::kP2pPointToPoint,
                                  PowerLevel::kHighPower,
                                  /*include_dct_advertisement=*/false,
                                  mediums::DiscoveredPeripheralCallback{}));
  env_.Stop();
}

TEST_F(BleTest, StartFastScanningDiscoverAndLostPeripheral) {
  env_.Start();
  BluetoothRadio radio_a;
  BluetoothRadio radio_b;
  Ble ble_a(radio_a);
  Ble ble_b(radio_b);
  radio_a.Enable();
  radio_b.Enable();
  auto advertisement_bytes = ByteArray::FromStringView(kAdvertisementString);
  CountDownLatch found_latch(1);
  CountDownLatch lost_latch(1);

  ble_b.StartAdvertising(kServiceIDA, PowerLevel::kHighPower,
                         Ble::AdvertisingType::kFast, advertisement_bytes);

  ble_a.StartScanning(
      kServiceIDA, Pcp::kP2pPointToPoint, PowerLevel::kHighPower,
      /*include_dct_advertisement=*/false,
      mediums::DiscoveredPeripheralCallback{
          .peripheral_discovered_cb =
              [&found_latch](BlePeripheral peripheral,
                             const std::string& service_id,
                             const ByteArray& advertisement_bytes,
                             bool fast_advertisement) {
                EXPECT_TRUE(fast_advertisement);
                found_latch.CountDown();
              },
          .peripheral_lost_cb =
              [&lost_latch](
                  BlePeripheral peripheral, const std::string& service_id,
                  const ByteArray& advertisement_bytes,
                  bool fast_advertisement) { lost_latch.CountDown(); },
      });

  EXPECT_TRUE(found_latch.Await(kWaitDuration).result());

  ble_b.StopAdvertising(kServiceIDA);

  // Wait for a while (2 times delay) to let the alarm occur twice and
  // `ProcessLostGattAdvertisements` twice to lost periperal.
  SystemClock::Sleep(absl::Milliseconds(kPeripheralLostTimeoutInMillis) * 2);

  EXPECT_TRUE(lost_latch.Await(kWaitDuration).result());

  ble_a.StopScanning(kServiceIDA);
  env_.Stop();
}

TEST_F(BleTest, StartFastScanningDiscoverButNoPeripheralLostAfterStopScanning) {
  env_.Start();
  BluetoothRadio radio_a;
  BluetoothRadio radio_b;
  Ble ble_a(radio_a);
  Ble ble_b(radio_b);
  radio_a.Enable();
  radio_b.Enable();
  auto advertisement_bytes = ByteArray::FromStringView(kAdvertisementString);
  CountDownLatch found_latch(1);
  CountDownLatch lost_latch(1);

  ble_b.StartAdvertising(kServiceIDA, PowerLevel::kHighPower,
                         Ble::AdvertisingType::kFast, advertisement_bytes);

  ble_a.StartScanning(
      kServiceIDA, Pcp::kP2pPointToPoint, PowerLevel::kHighPower,
      /*include_dct_advertisement=*/false,
      mediums::DiscoveredPeripheralCallback{
          .peripheral_discovered_cb =
              [&found_latch](BlePeripheral peripheral,
                             const std::string& service_id,
                             const ByteArray& advertisement_bytes,
                             bool fast_advertisement) {
                EXPECT_TRUE(fast_advertisement);
                found_latch.CountDown();
              },
          .peripheral_lost_cb =
              [&lost_latch](
                  BlePeripheral peripheral, const std::string& service_id,
                  const ByteArray& advertisement_bytes,
                  bool fast_advertisement) { lost_latch.CountDown(); },
      });

  EXPECT_TRUE(found_latch.Await(kWaitDuration).result());

  ble_b.StopAdvertising(kServiceIDA);
  ble_a.StopScanning(kServiceIDA);

  // Don't receive lost peripheral callback because we have stopped scanning and
  // cancelled the alarm.
  EXPECT_FALSE(lost_latch.Await(kWaitDuration).result());

  env_.Stop();
}

TEST_F(BleTest, StartScanningDiscoverAndLostPeripheral) {
  env_.Start();
  BluetoothRadio radio_a;
  BluetoothRadio radio_b;
  Ble ble_a(radio_a);
  Ble ble_b(radio_b);
  radio_a.Enable();
  radio_b.Enable();
  auto advertisement_bytes = ByteArray::FromStringView(kAdvertisementString);
  CountDownLatch found_latch(1);
  CountDownLatch lost_latch(1);

  ble_b.StartAdvertising(kServiceIDA, PowerLevel::kHighPower,
                         Ble::AdvertisingType::kRegular, advertisement_bytes);

  ble_a.StartScanning(
      kServiceIDA, Pcp::kP2pPointToPoint, PowerLevel::kHighPower,
      /*include_dct_advertisement=*/false,
      mediums::DiscoveredPeripheralCallback{
          .peripheral_discovered_cb =
              [&found_latch](BlePeripheral peripheral,
                             const std::string& service_id,
                             const ByteArray& advertisement_bytes,
                             bool fast_advertisement) {
                EXPECT_FALSE(fast_advertisement);
                found_latch.CountDown();
              },
          .peripheral_lost_cb =
              [&lost_latch](
                  BlePeripheral peripheral, const std::string& service_id,
                  const ByteArray& advertisement_bytes,
                  bool fast_advertisement) { lost_latch.CountDown(); },
      });

  EXPECT_TRUE(found_latch.Await(kWaitDuration).result());

  ble_b.StopAdvertising(kServiceIDA);

  // Wait for a while (2 times delay) to let the alarm occur twice and
  // `ProcessLostGattAdvertisements` twice to lost periperal.
  SystemClock::Sleep(absl::Milliseconds(kPeripheralLostTimeoutInMillis) * 2);

  EXPECT_TRUE(lost_latch.Await(kWaitDuration).result());

  ble_a.StopScanning(kServiceIDA);
  env_.Stop();
}

TEST_F(BleTest, StartScanningDiscoverButNoPeripheralLostAfterStopScanning) {
  env_.Start();
  BluetoothRadio radio_a;
  BluetoothRadio radio_b;
  Ble ble_a(radio_a);
  Ble ble_b(radio_b);
  radio_a.Enable();
  radio_b.Enable();
  auto advertisement_bytes = ByteArray::FromStringView(kAdvertisementString);
  CountDownLatch found_latch(1);
  CountDownLatch lost_latch(1);

  ble_b.StartAdvertising(kServiceIDA, PowerLevel::kHighPower,
                         Ble::AdvertisingType::kRegular, advertisement_bytes);

  ble_a.StartScanning(
      kServiceIDA, Pcp::kP2pPointToPoint, PowerLevel::kHighPower,
      /*include_dct_advertisement=*/false,
      mediums::DiscoveredPeripheralCallback{
          .peripheral_discovered_cb =
              [&found_latch](BlePeripheral peripheral,
                             const std::string& service_id,
                             const ByteArray& advertisement_bytes,
                             bool fast_advertisement) {
                EXPECT_FALSE(fast_advertisement);
                found_latch.CountDown();
              },
          .peripheral_lost_cb =
              [&lost_latch](
                  BlePeripheral peripheral, const std::string& service_id,
                  const ByteArray& advertisement_bytes,
                  bool fast_advertisement) { lost_latch.CountDown(); },
      });

  EXPECT_TRUE(found_latch.Await(kWaitDuration).result());

  ble_b.StopAdvertising(kServiceIDA);
  ble_a.StopScanning(kServiceIDA);

  // Don't receive lost peripheral callback because we have stopped scanning and
  // cancelled the alarm.
  EXPECT_FALSE(lost_latch.Await(kWaitDuration).result());

  env_.Stop();
}

TEST_F(BleTest, CanStartAndStopLegacyAdvertising) {
  env_.Start();
  BluetoothRadio radio_a;
  Ble ble_a{radio_a};
  radio_a.Enable();
  std::string service_id(kServiceIDA);
  EXPECT_TRUE(ble_a.StartLegacyAdvertising(service_id, kLocalEndpointId,
                                           kFastAdvertisementServiceUuid));
  EXPECT_FALSE(ble_a.IsAdvertising(service_id));
  EXPECT_TRUE(ble_a.IsAdvertisingForLegacyDevice(service_id));
  EXPECT_TRUE(ble_a.StopLegacyAdvertising(service_id));
  EXPECT_FALSE(ble_a.IsAdvertisingForLegacyDevice(service_id));
  env_.Stop();
}

TEST_F(BleTest, CanNotStartLegacyAdvertisingWhenRadioNotEnabled) {
  env_.Start();
  BluetoothRadio radio_a;
  Ble ble_a{radio_a};
  radio_a.Disable();
  std::string service_id(kServiceIDA);
  EXPECT_FALSE(ble_a.StartLegacyAdvertising(service_id, kLocalEndpointId,
                                            kFastAdvertisementServiceUuid));
  EXPECT_FALSE(ble_a.IsAdvertisingForLegacyDevice(service_id));
  env_.Stop();
}

TEST_F(BleTest, CanNotStopLegacyAdvertisingForNonExistingServiceId) {
  env_.Start();
  BluetoothRadio radio_a;
  Ble ble_a{radio_a};
  radio_a.Enable();
  std::string service_id(kServiceIDA);
  EXPECT_FALSE(ble_a.IsAdvertisingForLegacyDevice(service_id));
  EXPECT_FALSE(ble_a.StopLegacyAdvertising(service_id));
  EXPECT_TRUE(ble_a.StartLegacyAdvertising(service_id, kLocalEndpointId,
                                           kFastAdvertisementServiceUuid));
  EXPECT_TRUE(ble_a.IsAdvertisingForLegacyDevice(service_id));
  EXPECT_TRUE(ble_a.StopLegacyAdvertising(service_id));
  EXPECT_FALSE(ble_a.IsAdvertisingForLegacyDevice(service_id));
  env_.Stop();
}

TEST_F(BleTest, StartLegacyAdvertisingNotBlockedByRegularAdvertising) {
  env_.Start();
  BluetoothRadio radio_a;
  Ble ble_a{radio_a};
  radio_a.Enable();
  std::string service_id(kServiceIDA);
  auto advertisement_bytes = ByteArray::FromStringView(kAdvertisementString);

  ble_a.StartAdvertising(kServiceIDA, PowerLevel::kHighPower,
                         Ble::AdvertisingType::kRegular, advertisement_bytes);
  EXPECT_TRUE(ble_a.IsAdvertising(service_id));
  EXPECT_TRUE(ble_a.StartLegacyAdvertising(service_id, kLocalEndpointId,
                                           kFastAdvertisementServiceUuid));
  EXPECT_TRUE(ble_a.IsAdvertisingForLegacyDevice(service_id));
  ble_a.StopAdvertising(kServiceIDA);
  env_.Stop();
}

TEST_F(BleTest, DuplicateStartLegacyAdvertisingReturnsFalse) {
  env_.Start();
  BluetoothRadio radio_a;
  Ble ble_a{radio_a};
  radio_a.Enable();
  std::string service_id(kServiceIDA);
  EXPECT_FALSE(ble_a.IsAdvertisingForLegacyDevice(service_id));
  EXPECT_TRUE(ble_a.StartLegacyAdvertising(service_id, kLocalEndpointId,
                                           kFastAdvertisementServiceUuid));
  EXPECT_TRUE(ble_a.IsAdvertisingForLegacyDevice(service_id));
  EXPECT_FALSE(ble_a.StartLegacyAdvertising(service_id, kLocalEndpointId,
                                            kFastAdvertisementServiceUuid));
  EXPECT_TRUE(ble_a.StopLegacyAdvertising(service_id));
  EXPECT_FALSE(ble_a.IsAdvertisingForLegacyDevice(service_id));
  env_.Stop();
}

TEST_F(BleTest, HandleLegacyAdvertising) {
  env_.SetFeatureFlags(
      {FeatureFlags{.enable_invoking_legacy_device_discovered_cb = true}});
  env_.Start();
  BluetoothRadio radio_a;
  BluetoothRadio radio_b;
  Ble ble_a(radio_a);
  Ble ble_b(radio_b);
  radio_a.Enable();
  radio_b.Enable();
  auto advertisement_bytes = ByteArray::FromStringView(kAdvertisementString);
  CountDownLatch legacy_device_found_latch(1);

  ble_b.StartLegacyAdvertising(kServiceIDA, kLocalEndpointId,
                               kFastAdvertisementServiceUuid);
  EXPECT_FALSE(ble_a.IsAdvertisingForLegacyDevice(kServiceIDA));
  std::string legacy_service_id("NearbySharing");
  EXPECT_TRUE(ble_a.StartScanning(
      legacy_service_id, Pcp::kP2pPointToPoint, PowerLevel::kHighPower,
      /*include_dct_advertisement=*/false,
      mediums::DiscoveredPeripheralCallback{
          .peripheral_discovered_cb =
              [](BlePeripheral peripheral, const std::string& service_id,
                 const ByteArray& advertisement_bytes,
                 bool fast_advertisement) {
                FAIL() << "Legacy device shouldn't be reported here.";
              },
          .legacy_device_discovered_cb =
              [&legacy_device_found_latch]() {
                legacy_device_found_latch.CountDown();
              },
      }));

  EXPECT_TRUE(legacy_device_found_latch.Await(kWaitDuration).result());
  ble_b.StopAdvertising(kServiceIDA);
  EXPECT_TRUE(ble_a.StopScanning(legacy_service_id));
  env_.Stop();
}

TEST_F(BleTest, CanStartAsyncScanning) {
  env_.SetFeatureFlags({FeatureFlags{.enable_ble_v2_async_scanning = true}});
  env_.Start();
  BluetoothRadio radio_a;
  BluetoothRadio radio_b;
  Ble ble_a(radio_a);
  Ble ble_b(radio_b);
  radio_a.Enable();
  radio_b.Enable();
  auto advertisement_bytes = ByteArray::FromStringView(kAdvertisementString);
  CountDownLatch found_latch(1);

  ble_b.StartAdvertising(kServiceIDA, PowerLevel::kHighPower,
                         Ble::AdvertisingType::kRegular, advertisement_bytes);

  EXPECT_TRUE(ble_a.StartScanning(
      kServiceIDA, Pcp::kP2pPointToPoint, PowerLevel::kHighPower,
      /*include_dct_advertisement=*/false,
      mediums::DiscoveredPeripheralCallback{
          .peripheral_discovered_cb =
              [&found_latch](BlePeripheral peripheral,
                             const std::string& service_id,
                             const ByteArray& advertisement_bytes,
                             bool fast_advertisement) {
                EXPECT_FALSE(fast_advertisement);
                found_latch.CountDown();
              },
      }));

  EXPECT_TRUE(found_latch.Await(kWaitDuration).result());
  ble_b.StopAdvertising(kServiceIDA);
  EXPECT_TRUE(ble_a.StopScanning(kServiceIDA));
  env_.Stop();
}

TEST_F(BleTest, StartAsyncScanningWithPlatformErrors) {
  env_.SetFeatureFlags({FeatureFlags{.enable_ble_v2_async_scanning = true}});
  env_.Start();
  BluetoothRadio radio_a;
  BluetoothRadio radio_b;
  Ble ble_a(radio_a);
  Ble ble_b(radio_b);
  radio_a.Enable();
  radio_b.Enable();
  auto advertisement_bytes = ByteArray::FromStringView(kAdvertisementString);
  CountDownLatch found_latch(1);

  ble_b.StartAdvertising(kServiceIDA, PowerLevel::kHighPower,
                         Ble::AdvertisingType::kRegular, advertisement_bytes);

  // Disable radio a to simulate platform error.
  radio_a.Disable();
  EXPECT_FALSE(ble_a.StartScanning(
      kServiceIDA, Pcp::kP2pPointToPoint, PowerLevel::kHighPower,
      /*include_dct_advertisement=*/false,
      mediums::DiscoveredPeripheralCallback{
          .peripheral_discovered_cb =
              [&found_latch](BlePeripheral peripheral,
                             const std::string& service_id,
                             const ByteArray& advertisement_bytes,
                             bool fast_advertisement) {
                EXPECT_FALSE(fast_advertisement);
                found_latch.CountDown();
              },
      }));

  radio_a.Enable();
  EXPECT_TRUE(ble_a.StartScanning(
      kServiceIDA, Pcp::kP2pPointToPoint, PowerLevel::kHighPower,
      /*include_dct_advertisement=*/false,
      mediums::DiscoveredPeripheralCallback{
          .peripheral_discovered_cb =
              [&found_latch](BlePeripheral peripheral,
                             const std::string& service_id,
                             const ByteArray& advertisement_bytes,
                             bool fast_advertisement) {
                EXPECT_FALSE(fast_advertisement);
                found_latch.CountDown();
              },
      }));

  EXPECT_TRUE(found_latch.Await(kWaitDuration).result());
  ble_b.StopAdvertising(kServiceIDA);

  EXPECT_TRUE(ble_a.StopScanning(kServiceIDA));

  // Should return false the second time, as we removed service ID from the map.
  EXPECT_FALSE(ble_a.StopScanning(kServiceIDA));
  env_.Stop();
}

TEST_F(BleTest, StartAsyncScanningDiscoverAndLostPeripheral) {
  env_.SetFeatureFlags({FeatureFlags{.enable_ble_v2_async_scanning = true}});
  env_.Start();
  BluetoothRadio radio_a;
  BluetoothRadio radio_b;
  Ble ble_a(radio_a);
  Ble ble_b(radio_b);
  radio_a.Enable();
  radio_b.Enable();
  auto advertisement_bytes = ByteArray::FromStringView(kAdvertisementString);
  CountDownLatch found_latch(1);
  CountDownLatch lost_latch(1);

  ble_b.StartAdvertising(kServiceIDA, PowerLevel::kHighPower,
                         Ble::AdvertisingType::kRegular, advertisement_bytes);

  EXPECT_TRUE(ble_a.StartScanning(
      kServiceIDA, Pcp::kP2pPointToPoint, PowerLevel::kHighPower,
      /*include_dct_advertisement=*/false,
      mediums::DiscoveredPeripheralCallback{
          .peripheral_discovered_cb =
              [&found_latch](BlePeripheral peripheral,
                             const std::string& service_id,
                             const ByteArray& advertisement_bytes,
                             bool fast_advertisement) {
                EXPECT_FALSE(fast_advertisement);
                found_latch.CountDown();
              },
          .peripheral_lost_cb =
              [&lost_latch](
                  BlePeripheral peripheral, const std::string& service_id,
                  const ByteArray& advertisement_bytes,
                  bool fast_advertisement) { lost_latch.CountDown(); },
      }));

  EXPECT_TRUE(found_latch.Await(kWaitDuration).result());
  EXPECT_TRUE(ble_b.StopAdvertising(kServiceIDA));

  // Wait for a while (2 times delay) to let the alarm occur twice and
  // `ProcessLostGattAdvertisements` twice to lost periperal.
  SystemClock::Sleep(absl::Milliseconds(kPeripheralLostTimeoutInMillis) * 2);

  EXPECT_TRUE(lost_latch.Await(kWaitDuration).result());

  EXPECT_TRUE(ble_a.StopScanning(kServiceIDA));
  env_.Stop();
}

TEST_F(BleTest,
       StartAsyncScanningDiscoverButNoPeripheralLostAfterStopScanning) {
  env_.SetFeatureFlags({FeatureFlags{.enable_ble_v2_async_scanning = true}});
  env_.Start();
  BluetoothRadio radio_a;
  BluetoothRadio radio_b;
  Ble ble_a(radio_a);
  Ble ble_b(radio_b);
  radio_a.Enable();
  radio_b.Enable();
  auto advertisement_bytes = ByteArray::FromStringView(kAdvertisementString);
  CountDownLatch found_latch(1);
  CountDownLatch lost_latch(1);

  ble_b.StartAdvertising(kServiceIDA, PowerLevel::kHighPower,
                         Ble::AdvertisingType::kRegular, advertisement_bytes);

  EXPECT_TRUE(ble_a.StartScanning(
      kServiceIDA, Pcp::kP2pPointToPoint, PowerLevel::kHighPower,
      /*include_dct_advertisement=*/false,
      mediums::DiscoveredPeripheralCallback{
          .peripheral_discovered_cb =
              [&found_latch](BlePeripheral peripheral,
                             const std::string& service_id,
                             const ByteArray& advertisement_bytes,
                             bool fast_advertisement) {
                EXPECT_FALSE(fast_advertisement);
                found_latch.CountDown();
              },
          .peripheral_lost_cb =
              [&lost_latch](
                  BlePeripheral peripheral, const std::string& service_id,
                  const ByteArray& advertisement_bytes,
                  bool fast_advertisement) { lost_latch.CountDown(); },
      }));

  EXPECT_TRUE(found_latch.Await(kWaitDuration).result());

  EXPECT_TRUE(ble_b.StopAdvertising(kServiceIDA));
  EXPECT_TRUE(ble_a.StopScanning(kServiceIDA));

  // Don't receive lost peripheral callback because we have stopped scanning and
  // cancelled the alarm.
  EXPECT_FALSE(lost_latch.Await(kWaitDuration).result());

  env_.Stop();
}

TEST_F(BleTest, CanStartStopMultipleAsyncScanningWithDifferentServiceIds) {
  env_.SetFeatureFlags({FeatureFlags{.enable_ble_v2_async_scanning = true}});
  env_.Start();
  BluetoothRadio radio_scanner;
  BluetoothRadio radio_advertiser_a;
  BluetoothRadio radio_advertiser_b;
  Ble ble_scanner(radio_scanner);
  Ble ble_advertiser_a(radio_advertiser_a);
  Ble ble_advertiser_b(radio_advertiser_b);
  radio_scanner.Enable();
  radio_advertiser_a.Enable();
  radio_advertiser_b.Enable();
  auto advertisement_bytes_a = ByteArray::FromStringView(kAdvertisementString);
  auto advertisement_bytes_b = ByteArray::FromStringView(kAdvertisementStringB);
  CountDownLatch found_latch_a(1);
  CountDownLatch found_latch_b(1);

  ble_advertiser_a.StartAdvertising(kServiceIDA, PowerLevel::kHighPower,
                                    Ble::AdvertisingType::kRegular,
                                    advertisement_bytes_a);
  ble_advertiser_b.StartAdvertising(kServiceIDB, PowerLevel::kHighPower,
                                    Ble::AdvertisingType::kRegular,
                                    advertisement_bytes_b);

  ble_scanner.StartScanning(
      kServiceIDA, Pcp::kP2pPointToPoint, PowerLevel::kHighPower,
      /*include_dct_advertisement=*/false,
      mediums::DiscoveredPeripheralCallback{
          .peripheral_discovered_cb =
              [&found_latch_a](BlePeripheral peripheral,
                               const std::string& service_id,
                               const ByteArray& advertisement_bytes,
                               bool fast_advertisement) {
                EXPECT_EQ(service_id, kServiceIDA);
                EXPECT_FALSE(fast_advertisement);
                found_latch_a.CountDown();
              },
      });

  ble_scanner.StartScanning(
      kServiceIDB, Pcp::kP2pPointToPoint, PowerLevel::kHighPower,
      /*include_dct_advertisement=*/false,
      mediums::DiscoveredPeripheralCallback{
          .peripheral_discovered_cb =
              [&found_latch_b](BlePeripheral peripheral,
                               const std::string& service_id,
                               const ByteArray& advertisement_bytes,
                               bool fast_advertisement) {
                EXPECT_EQ(service_id, kServiceIDB);
                EXPECT_FALSE(fast_advertisement);
                found_latch_b.CountDown();
              },
      });

  EXPECT_TRUE(found_latch_a.Await(kWaitDuration).result());
  EXPECT_TRUE(found_latch_b.Await(kWaitDuration).result());
  EXPECT_TRUE(ble_scanner.StopScanning(kServiceIDA));
  EXPECT_TRUE(ble_scanner.StopScanning(kServiceIDB));
  env_.Stop();
}

TEST_F(BleTest, StartMultipleAsyncScanningDiscoverAndLostPeripheral) {
  env_.SetFeatureFlags({FeatureFlags{.enable_ble_v2_async_scanning = true}});
  env_.Start();
  BluetoothRadio radio_scanner;
  BluetoothRadio radio_advertiser_a;
  BluetoothRadio radio_advertiser_b;
  Ble ble_scanner(radio_scanner);
  Ble ble_advertiser_a(radio_advertiser_a);
  Ble ble_advertiser_b(radio_advertiser_b);
  radio_scanner.Enable();
  radio_advertiser_a.Enable();
  radio_advertiser_b.Enable();
  auto advertisement_bytes_a = ByteArray::FromStringView(kAdvertisementString);
  auto advertisement_bytes_b = ByteArray::FromStringView(kAdvertisementStringB);
  CountDownLatch found_latch_a(1);
  CountDownLatch found_latch_b(1);
  CountDownLatch lost_latch_a(1);
  CountDownLatch lost_latch_b(1);

  ble_advertiser_a.StartAdvertising(kServiceIDA, PowerLevel::kHighPower,
                                    Ble::AdvertisingType::kRegular,
                                    advertisement_bytes_a);
  ble_advertiser_b.StartAdvertising(kServiceIDB, PowerLevel::kHighPower,
                                    Ble::AdvertisingType::kRegular,
                                    advertisement_bytes_b);

  ble_scanner.StartScanning(
      kServiceIDA, Pcp::kP2pPointToPoint, PowerLevel::kHighPower,
      /*include_dct_advertisement=*/false,
      mediums::DiscoveredPeripheralCallback{
          .peripheral_discovered_cb =
              [&found_latch_a](BlePeripheral peripheral,
                               const std::string& service_id,
                               const ByteArray& advertisement_bytes,
                               bool fast_advertisement) {
                EXPECT_EQ(service_id, kServiceIDA);
                EXPECT_FALSE(fast_advertisement);
                found_latch_a.CountDown();
              },
          .peripheral_lost_cb =
              [&lost_latch_a](BlePeripheral peripheral,
                              const std::string& service_id,
                              const ByteArray& advertisement_bytes,
                              bool fast_advertisement) {
                EXPECT_EQ(service_id, kServiceIDA);
                EXPECT_FALSE(fast_advertisement);
                lost_latch_a.CountDown();
              },
      });

  ble_scanner.StartScanning(
      kServiceIDB, Pcp::kP2pPointToPoint, PowerLevel::kHighPower,
      /*include_dct_advertisement=*/false,
      mediums::DiscoveredPeripheralCallback{
          .peripheral_discovered_cb =
              [&found_latch_b](BlePeripheral peripheral,
                               const std::string& service_id,
                               const ByteArray& advertisement_bytes,
                               bool fast_advertisement) {
                EXPECT_EQ(service_id, kServiceIDB);
                EXPECT_FALSE(fast_advertisement);
                found_latch_b.CountDown();
              },
          .peripheral_lost_cb =
              [&lost_latch_b](BlePeripheral peripheral,
                              const std::string& service_id,
                              const ByteArray& advertisement_bytes,
                              bool fast_advertisement) {
                EXPECT_EQ(service_id, kServiceIDB);
                EXPECT_FALSE(fast_advertisement);
                lost_latch_b.CountDown();
              },
      });

  EXPECT_TRUE(found_latch_a.Await(kWaitDuration).result());
  EXPECT_TRUE(found_latch_b.Await(kWaitDuration).result());

  EXPECT_TRUE(ble_advertiser_a.StopAdvertising(kServiceIDA));

  // Wait for a while (2 times delay) to let the alarm occur twice and
  // `ProcessLostGattAdvertisements` twice to lost periperal.
  SystemClock::Sleep(absl::Milliseconds(kPeripheralLostTimeoutInMillis) * 2);

  EXPECT_TRUE(lost_latch_a.Await(kWaitDuration).result());

  EXPECT_TRUE(ble_advertiser_b.StopAdvertising(kServiceIDB));

  // Wait for a while (2 times delay) to let the alarm occur twice and
  // `ProcessLostGattAdvertisements` twice to lost periperal.
  SystemClock::Sleep(absl::Milliseconds(kPeripheralLostTimeoutInMillis) * 2);

  EXPECT_TRUE(lost_latch_b.Await(kWaitDuration).result());

  EXPECT_TRUE(ble_scanner.StopScanning(kServiceIDA));
  EXPECT_TRUE(ble_scanner.StopScanning(kServiceIDB));
  env_.Stop();
}

TEST_F(BleTest,
       StopOneAsyncScanningSessionDoesNotCancelLostAlarmForOtherSessions) {
  env_.SetFeatureFlags({FeatureFlags{.enable_ble_v2_async_scanning = true}});
  env_.Start();
  BluetoothRadio radio_scanner;
  BluetoothRadio radio_advertiser_a;
  BluetoothRadio radio_advertiser_b;
  Ble ble_scanner(radio_scanner);
  Ble ble_advertiser_a(radio_advertiser_a);
  Ble ble_advertiser_b(radio_advertiser_b);
  radio_scanner.Enable();
  radio_advertiser_a.Enable();
  radio_advertiser_b.Enable();
  auto advertisement_bytes_a = ByteArray::FromStringView(kAdvertisementString);
  auto advertisement_bytes_b = ByteArray::FromStringView(kAdvertisementStringB);
  CountDownLatch found_latch_a(1);
  CountDownLatch found_latch_b(1);
  CountDownLatch lost_latch_b(1);

  ble_advertiser_a.StartAdvertising(kServiceIDA, PowerLevel::kHighPower,
                                    Ble::AdvertisingType::kRegular,
                                    advertisement_bytes_a);
  ble_advertiser_b.StartAdvertising(kServiceIDB, PowerLevel::kHighPower,
                                    Ble::AdvertisingType::kRegular,
                                    advertisement_bytes_b);

  ble_scanner.StartScanning(
      kServiceIDA, Pcp::kP2pPointToPoint, PowerLevel::kHighPower,
      /*include_dct_advertisement=*/false,
      mediums::DiscoveredPeripheralCallback{
          .peripheral_discovered_cb =
              [&found_latch_a](
                  BlePeripheral peripheral, const std::string& service_id,
                  const ByteArray& advertisement_bytes,
                  bool fast_advertisement) { found_latch_a.CountDown(); },
      });

  ble_scanner.StartScanning(
      kServiceIDB, Pcp::kP2pPointToPoint, PowerLevel::kHighPower,
      /*include_dct_advertisement=*/false,
      mediums::DiscoveredPeripheralCallback{
          .peripheral_discovered_cb =
              [&found_latch_b](
                  BlePeripheral peripheral, const std::string& service_id,
                  const ByteArray& advertisement_bytes,
                  bool fast_advertisement) { found_latch_b.CountDown(); },
          .peripheral_lost_cb =
              [&lost_latch_b](
                  BlePeripheral peripheral, const std::string& service_id,
                  const ByteArray& advertisement_bytes,
                  bool fast_advertisement) { lost_latch_b.CountDown(); },
      });

  ASSERT_TRUE(found_latch_a.Await(kWaitDuration).result());
  ASSERT_TRUE(found_latch_b.Await(kWaitDuration).result());

  // Stop scanning for service A only.
  ASSERT_TRUE(ble_scanner.StopScanning(kServiceIDA));

  // Stop advertiser B. Lost alarm must remain active for service B.
  ASSERT_TRUE(ble_advertiser_b.StopAdvertising(kServiceIDB));

  // Wait to allow the recurring lost alarm to process lost advertisements.
  SystemClock::Sleep(absl::Milliseconds(kPeripheralLostTimeoutInMillis) * 2);

  // Service B should still receive the peripheral lost callback.
  EXPECT_TRUE(lost_latch_b.Await(kWaitDuration).result());

  EXPECT_TRUE(ble_scanner.StopScanning(kServiceIDB));
  env_.Stop();
}

TEST_F(BleTest, RetrieveBlePeripheralIdFromNativeId) {
  env_.Start();
  BluetoothRadio radio;
  MacAddress mac_address = radio.GetBluetoothAdapter().GetAddress();
  Ble ble(radio);
  env_.Sync(false);
  EXPECT_TRUE(ble.RetrieveBlePeripheralFromNativeId(mac_address.ToString())
                  .has_value());

  EXPECT_FALSE(
      ble.RetrieveBlePeripheralFromNativeId("FF:FF:FF:FF:FF:DE").has_value());
  env_.Stop();
}

TEST_F(BleTest, StartAcceptingL2capConnectionsSurvivesInvalidPacketsAndEof) {
  env_.Start();
  env_.SetBleL2capAvailable(true);
  BluetoothRadio radio_client;
  BluetoothRadio radio_server;
  Ble ble_client{radio_client};
  Ble ble_server{radio_server};
  radio_client.Enable();
  radio_server.Enable();
  std::string service_id(kServiceIDA);
  ByteArray advertisement_bytes =
      ByteArray::FromStringView(kAdvertisementString);
  absl::Notification accept_notification;

  std::unique_ptr<mediums::BleSocket> socket_for_server;
  ErrorOr<int> psm_result = ble_server.StartAcceptingL2capConnections(
      service_id,
      [&](std::unique_ptr<mediums::BleSocket> socket, const std::string&) {
        socket_for_server = std::move(socket);
        accept_notification.Notify();
      });
  ASSERT_TRUE(psm_result.has_value());

  std::optional<BlePeripheral> discovered_peripheral =
      StartAdvertisingAndDiscoverPeripheral(ble_server, ble_client, service_id,
                                            advertisement_bytes);
  ASSERT_TRUE(discovered_peripheral.has_value());
  discovered_peripheral->SetPsm(psm_result.value());

  CancellationFlag flag;

  // 1. Immediate EOF without sending any command byte.
  ErrorOr<BleL2capSocket> eof_client =
      ble_client.ConnectOverL2cap(service_id, *discovered_peripheral, &flag);
  ASSERT_TRUE(eof_client.has_value());
  EXPECT_TRUE(eof_client.value().Close().Ok());

  // 2. Truncated kRequestAdvertisement packet (1 byte followed by EOF).
  ErrorOr<BleL2capSocket> truncated_client =
      ble_client.ConnectOverL2cap(service_id, *discovered_peripheral, &flag);
  ASSERT_TRUE(truncated_client.has_value());
  EXPECT_TRUE(truncated_client.value()
                  .GetOutputStream()
                  .Write(absl::string_view("\x01", 1))
                  .Ok());
  EXPECT_TRUE(truncated_client.value().Close().Ok());

  // 3. Unexpected L2CAP command (not kRequestDataConnection).
  ErrorOr<BleL2capSocket> wrong_cmd_client =
      ble_client.ConnectOverL2cap(service_id, *discovered_peripheral, &flag);
  ASSERT_TRUE(wrong_cmd_client.has_value());
  EXPECT_TRUE(
      wrong_cmd_client.value()
          .GetOutputStream()
          .Write(mediums::BleL2capPacket::ByteArrayForDataConnectionReady()
                     .AsStringView())
          .Ok());
  EXPECT_TRUE(wrong_cmd_client.value().Close().Ok());

  // 4. Subsequent valid L2CAP data connection still succeeds because the
  // server accept loop continued rather than exiting on invalid packets.
  ErrorOr<std::unique_ptr<mediums::BleSocket>> valid_client =
      ble_client.ConnectOverL2cap2(service_id, *discovered_peripheral, &flag);
  ASSERT_TRUE(
      accept_notification.WaitForNotificationWithTimeout(kWaitDuration));
  ASSERT_TRUE(valid_client.has_value());
  ASSERT_NE(valid_client.value(), nullptr);
  EXPECT_TRUE(valid_client.value()->IsValid());
  ASSERT_NE(socket_for_server, nullptr);
  EXPECT_TRUE(socket_for_server->IsValid());

  EXPECT_TRUE(valid_client.value()->Close().Ok());
  EXPECT_TRUE(socket_for_server->Close().Ok());
  EXPECT_TRUE(ble_client.StopScanning(service_id));
  EXPECT_TRUE(ble_server.StopAcceptingL2capConnections(service_id));
  EXPECT_TRUE(ble_server.StopAdvertising(service_id));
  env_.Stop();
}

TEST_F(BleTest, StartAcceptingL2capConnectionsHandlesMultipleConnections) {
  env_.Start();
  env_.SetBleL2capAvailable(true);
  BluetoothRadio radio_client;
  BluetoothRadio radio_server;
  Ble ble_client{radio_client};
  auto ble_server = std::make_unique<Ble>(radio_server);
  radio_client.Enable();
  radio_server.Enable();
  std::string service_id(kServiceIDA);
  ByteArray advertisement_bytes =
      ByteArray::FromStringView(kAdvertisementString);
  CountDownLatch accept_latch(2);

  absl::Mutex server_sockets_mutex;
  std::vector<std::unique_ptr<mediums::BleSocket>> server_sockets;
  ErrorOr<int> psm_result = ble_server->StartAcceptingL2capConnections(
      service_id,
      [&](std::unique_ptr<mediums::BleSocket> socket, const std::string&) {
        {
          absl::MutexLock lock(server_sockets_mutex);
          server_sockets.push_back(std::move(socket));
        }
        accept_latch.CountDown();
      });
  ASSERT_TRUE(psm_result.has_value());

  std::optional<BlePeripheral> discovered_peripheral =
      StartAdvertisingAndDiscoverPeripheral(*ble_server, ble_client, service_id,
                                            advertisement_bytes);
  ASSERT_TRUE(discovered_peripheral.has_value());
  discovered_peripheral->SetPsm(psm_result.value());

  CancellationFlag flag;
  ErrorOr<std::unique_ptr<mediums::BleSocket>> client_1 =
      ble_client.ConnectOverL2cap2(service_id, *discovered_peripheral, &flag);
  ASSERT_TRUE(client_1.has_value());
  ErrorOr<std::unique_ptr<mediums::BleSocket>> client_2 =
      ble_client.ConnectOverL2cap2(service_id, *discovered_peripheral, &flag);
  ASSERT_TRUE(client_2.has_value());
  EXPECT_TRUE(accept_latch.Await(kWaitDuration).result());

  std::unique_ptr<mediums::BleSocket> server_socket_1;
  std::unique_ptr<mediums::BleSocket> server_socket_2;
  {
    absl::MutexLock lock(server_sockets_mutex);
    ASSERT_EQ(server_sockets.size(), 2u);
    server_socket_1 = std::move(server_sockets[0]);
    server_socket_2 = std::move(server_sockets[1]);
  }
  ASSERT_NE(server_socket_1, nullptr);
  ASSERT_NE(server_socket_2, nullptr);

  // Close the first server socket while the second remains open.
  EXPECT_TRUE(server_socket_1->Close().Ok());
  EXPECT_TRUE(client_1.value()->Close().Ok());

  // Destroy `ble_server` while `server_socket_2` is still held by the caller.
  EXPECT_TRUE(ble_client.StopScanning(service_id));
  ble_server.reset();

  // Closing and destroying `server_socket_2` after `ble_server` is destroyed
  // must not dereference a dangling `Ble*` in the close notifier.
  EXPECT_TRUE(server_socket_2->Close().Ok());
  EXPECT_TRUE(client_2.value()->Close().Ok());
  server_socket_2.reset();
  env_.Stop();
}

TEST_F(BleTest, DestroyBleWhileL2capPeerStallsDuringValidation) {
  env_.Start();
  env_.SetBleL2capAvailable(true);
  BluetoothRadio radio_client;
  BluetoothRadio radio_server;
  Ble ble_client{radio_client};
  auto ble_server = std::make_unique<Ble>(radio_server);
  radio_client.Enable();
  radio_server.Enable();
  std::string service_id(kServiceIDA);
  ByteArray advertisement_bytes =
      ByteArray::FromStringView(kAdvertisementString);

  ErrorOr<int> psm_result = ble_server->StartAcceptingL2capConnections(
      service_id,
      [](std::unique_ptr<mediums::BleSocket> socket, const std::string&) {});
  ASSERT_TRUE(psm_result.has_value());

  std::optional<BlePeripheral> discovered_peripheral =
      StartAdvertisingAndDiscoverPeripheral(*ble_server, ble_client, service_id,
                                            advertisement_bytes);
  ASSERT_TRUE(discovered_peripheral.has_value());
  discovered_peripheral->SetPsm(psm_result.value());

  // Connect raw L2CAP socket without sending the validation packet so the
  // server's `ble-l2cap-accept` thread blocks inside
  // `ProcessIncomingL2capPacketValidation()`.
  CancellationFlag flag;
  ErrorOr<BleL2capSocket> stalled_client =
      ble_client.ConnectOverL2cap(service_id, *discovered_peripheral, &flag);
  ASSERT_TRUE(stalled_client.has_value());
  SystemClock::Sleep(absl::Milliseconds(50));

  EXPECT_TRUE(ble_client.StopScanning(service_id));
  // Destroying `ble_server` must close the tracked incoming socket, unblocking
  // `ProcessIncomingL2capPacketValidation()` so `~Ble()` does not hang.
  ble_server.reset();

  EXPECT_TRUE(stalled_client.value().Close().Ok());
  env_.Stop();
}

TEST_F(BleTest, ConcurrentL2capSocketCloseAndBleDestruction) {
  env_.Start();
  env_.SetBleL2capAvailable(true);
  BluetoothRadio radio_client;
  BluetoothRadio radio_server;
  Ble ble_client{radio_client};
  auto ble_server = std::make_unique<Ble>(radio_server);
  radio_client.Enable();
  radio_server.Enable();
  std::string service_id(kServiceIDA);
  ByteArray advertisement_bytes =
      ByteArray::FromStringView(kAdvertisementString);
  absl::Notification accept_notification;

  std::unique_ptr<mediums::BleSocket> server_socket;
  ErrorOr<int> psm_result = ble_server->StartAcceptingL2capConnections(
      service_id,
      [&](std::unique_ptr<mediums::BleSocket> socket, const std::string&) {
        server_socket = std::move(socket);
        accept_notification.Notify();
      });
  ASSERT_TRUE(psm_result.has_value());

  std::optional<BlePeripheral> discovered_peripheral =
      StartAdvertisingAndDiscoverPeripheral(*ble_server, ble_client, service_id,
                                            advertisement_bytes);
  ASSERT_TRUE(discovered_peripheral.has_value());
  discovered_peripheral->SetPsm(psm_result.value());

  CancellationFlag flag;
  ErrorOr<std::unique_ptr<mediums::BleSocket>> client_socket =
      ble_client.ConnectOverL2cap2(service_id, *discovered_peripheral, &flag);
  ASSERT_TRUE(client_socket.has_value());
  ASSERT_TRUE(
      accept_notification.WaitForNotificationWithTimeout(kWaitDuration));
  ASSERT_NE(server_socket, nullptr);

  EXPECT_TRUE(ble_client.StopScanning(service_id));

  SingleThreadExecutor close_executor;
  absl::Notification start_notification;
  absl::Notification close_done_notification;
  close_executor.Execute([&]() {
    start_notification.Notify();
    server_socket->Close();
    close_done_notification.Notify();
  });
  ASSERT_TRUE(start_notification.WaitForNotificationWithTimeout(kWaitDuration));
  ble_server.reset();
  EXPECT_TRUE(
      close_done_notification.WaitForNotificationWithTimeout(kWaitDuration));

  close_executor.Shutdown();
  EXPECT_TRUE(client_socket.value()->Close().Ok());
  env_.Stop();
}

TEST_F(
    BleTest,
    StopAcceptingL2capConnectionsUnblocksStalledValidationSuppressesCallback) {
  env_.Start();
  env_.SetBleL2capAvailable(true);
  BluetoothRadio radio_client;
  BluetoothRadio radio_server;
  Ble ble_client{radio_client};
  Ble ble_server{radio_server};
  radio_client.Enable();
  radio_server.Enable();
  std::string service_id(kServiceIDA);
  ByteArray advertisement_bytes =
      ByteArray::FromStringView(kAdvertisementString);

  ErrorOr<int> initial_psm = ble_server.StartAcceptingL2capConnections(
      service_id,
      [](std::unique_ptr<mediums::BleSocket> socket, const std::string&) {});
  ASSERT_TRUE(initial_psm.has_value());

  std::optional<BlePeripheral> discovered_peripheral =
      StartAdvertisingAndDiscoverPeripheral(ble_server, ble_client, service_id,
                                            advertisement_bytes);
  ASSERT_TRUE(discovered_peripheral.has_value());
  ASSERT_TRUE(ble_server.StopAcceptingL2capConnections(service_id));

  CancellationFlag flag;
  std::atomic<bool> unexpected_callback{false};
  // Repeat more than `kMaxConcurrentAcceptLoops` (5) times to verify that
  // `StopAcceptingL2capConnections` closes the in-flight unvalidated client
  // socket and unblocks the `ble-l2cap-accept` worker thread each time.
  for (int i = 0; i < 6; ++i) {
    ErrorOr<int> psm_result = ble_server.StartAcceptingL2capConnections(
        service_id,
        [&unexpected_callback](std::unique_ptr<mediums::BleSocket> socket,
                               const std::string&) {
          unexpected_callback.store(true, std::memory_order_relaxed);
        });
    ASSERT_TRUE(psm_result.has_value());
    discovered_peripheral->SetPsm(psm_result.value());

    ErrorOr<BleL2capSocket> stalled_client =
        ble_client.ConnectOverL2cap(service_id, *discovered_peripheral, &flag);
    ASSERT_TRUE(stalled_client.has_value());
    SystemClock::Sleep(absl::Milliseconds(20));

    EXPECT_TRUE(ble_server.StopAcceptingL2capConnections(service_id));

    // Sending the validation packet after `StopAcceptingL2capConnections` must
    // not invoke the accepted connection callback for the stopped service, and
    // the server-side unvalidated socket should already be closed (EOF).
    stalled_client.value().GetOutputStream().Write(
        mediums::BleL2capPacket::ByteArrayForRequestDataConnection()
            .AsStringView());
    ExceptionOr<ByteArray> read_result =
        stalled_client.value().GetInputStream().Read(1);
    EXPECT_TRUE(!read_result.ok() || read_result.result().Empty());
    EXPECT_TRUE(stalled_client.value().Close().Ok());
  }
  EXPECT_FALSE(unexpected_callback.load(std::memory_order_relaxed));

  // Verify the accept loop thread pool is not exhausted by accepting a valid
  // connection afterwards.
  absl::Notification accept_notification;
  std::unique_ptr<mediums::BleSocket> accepted_server_socket;
  ErrorOr<int> final_psm = ble_server.StartAcceptingL2capConnections(
      service_id,
      [&](std::unique_ptr<mediums::BleSocket> socket, const std::string&) {
        accepted_server_socket = std::move(socket);
        accept_notification.Notify();
      });
  ASSERT_TRUE(final_psm.has_value());
  discovered_peripheral->SetPsm(final_psm.value());

  ErrorOr<std::unique_ptr<mediums::BleSocket>> valid_client =
      ble_client.ConnectOverL2cap2(service_id, *discovered_peripheral, &flag);
  ASSERT_TRUE(valid_client.has_value());
  EXPECT_TRUE(
      accept_notification.WaitForNotificationWithTimeout(kWaitDuration));
  ASSERT_NE(accepted_server_socket, nullptr);

  EXPECT_TRUE(valid_client.value()->Close().Ok());
  EXPECT_TRUE(accepted_server_socket->Close().Ok());
  EXPECT_TRUE(ble_client.StopScanning(service_id));
  EXPECT_TRUE(ble_server.StopAcceptingL2capConnections(service_id));
  EXPECT_TRUE(ble_server.StopAdvertising(service_id));
  env_.Stop();
}

}  // namespace
}  // namespace connections
}  // namespace nearby
