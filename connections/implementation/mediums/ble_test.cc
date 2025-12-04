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

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "connections/implementation/flags/nearby_connections_feature_flags.h"
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
#include "internal/platform/expected.h"
#include "internal/platform/feature_flags.h"
#include "internal/platform/implementation/system_clock.h"
#include "internal/platform/logging.h"
#include "internal/platform/mac_address.h"
#include "internal/platform/medium_environment.h"

namespace nearby {
namespace connections {
namespace {

using FeatureFlags = FeatureFlags::Flags;

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
constexpr absl::string_view kServiceIDA =
    "com.google.location.nearby.apps.test.a";
constexpr absl::string_view kServiceIDB =
    "com.google.location.nearby.apps.test.b";
constexpr absl::string_view kAdvertisementString = "\x0a\x0b\x0c\x0d";
constexpr absl::string_view kAdvertisementStringB = "\x01\x02\x03\x04";
constexpr absl::string_view kLocalEndpointId = "local_endpoint_id";
constexpr absl::string_view kFastAdvertisementServiceUuid{"\xf3\xfe"};

class BleTest : public testing::TestWithParam<FeatureFlags> {
 public:
  void SetUp() override {
    NearbyFlags::GetInstance().OverrideInt64FlagValue(
        config_package_nearby::nearby_connections_feature::
            kBlePeripheralLostTimeoutMillis,
        /*ms=*/kPeripheralLostTimeoutInMillis);
  }

 protected:
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
  ByteArray advertisement_bytes{std::string(kAdvertisementString)};
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
  ByteArray advertisement_bytes{std::string(kAdvertisementString)};
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
  ByteArray advertisement_bytes{std::string(kAdvertisementString)};
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
  ByteArray advertisement_bytes{std::string(kAdvertisementString)};
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
  ByteArray advertisement_bytes{std::string(kAdvertisementString)};
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

  EXPECT_FALSE(ble_a.StartAdvertising(
      std::string(kServiceIDA), PowerLevel::kHighPower,
      Ble::AdvertisingType::kRegular, advertisement_bytes));
  env_.Stop();
}

TEST_F(BleTest, CanNotStartAdvertisingWhenRadioNotEnabled) {
  env_.Start();
  BluetoothRadio radio_a;
  Ble ble_a(radio_a);
  radio_a.Disable();
  ByteArray advertisement_bytes((std::string(kAdvertisementString)));

  EXPECT_FALSE(ble_a.StartAdvertising(
      std::string(kServiceIDA), PowerLevel::kHighPower,
      Ble::AdvertisingType::kRegular, advertisement_bytes));
  env_.Stop();
}

TEST_F(BleTest, CanStartAdvertisingWithDifferentPowerLevels) {
  env_.Start();
  BluetoothRadio radio_a;
  Ble ble_a(radio_a);
  radio_a.Enable();
  ByteArray advertisement_bytes((std::string(kAdvertisementString)));

  EXPECT_TRUE(ble_a.StartAdvertising(
      std::string(kServiceIDA), PowerLevel::kLowPower,
      Ble::AdvertisingType::kRegular, advertisement_bytes));
  EXPECT_TRUE(ble_a.StopAdvertising(std::string(kServiceIDA)));
  EXPECT_TRUE(ble_a.StartAdvertising(
      std::string(kServiceIDA), PowerLevel::kHighPower,
      Ble::AdvertisingType::kRegular, advertisement_bytes));
  EXPECT_TRUE(ble_a.StopAdvertising(std::string(kServiceIDA)));
  env_.Stop();
}

TEST_F(BleTest, CanStartAdvertisingWithDifferentAdvertisingTypes) {
  env_.Start();
  BluetoothRadio radio_a;
  Ble ble_a(radio_a);
  radio_a.Enable();
  ByteArray advertisement_bytes((std::string(kAdvertisementString)));

  EXPECT_TRUE(ble_a.StartAdvertising(
      std::string(kServiceIDA), PowerLevel::kHighPower,
      Ble::AdvertisingType::kRegular, advertisement_bytes));
  EXPECT_TRUE(ble_a.StopAdvertising(std::string(kServiceIDA)));
  EXPECT_TRUE(
      ble_a.StartAdvertising(std::string(kServiceIDA), PowerLevel::kHighPower,
                             Ble::AdvertisingType::kFast, advertisement_bytes));
  EXPECT_TRUE(ble_a.StopAdvertising(std::string(kServiceIDA)));
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
  ByteArray advertisement_bytes((std::string(kAdvertisementString)));
  CountDownLatch found_latch(1);

  ble_b.StartScanning(
      std::string(kServiceIDA), Pcp::kP2pPointToPoint, PowerLevel::kHighPower,
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

  EXPECT_TRUE(
      ble_a.StartAdvertising(std::string(kServiceIDA), PowerLevel::kHighPower,
                             Ble::AdvertisingType::kFast, advertisement_bytes));
  EXPECT_TRUE(found_latch.Await(kWaitDuration).result());
  EXPECT_TRUE(ble_a.StopAdvertising(std::string(kServiceIDA)));
  ble_b.StopScanning(std::string(kServiceIDA));
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
  ByteArray advertisement_bytes((std::string(kAdvertisementString)));
  CountDownLatch found_latch(1);

  ble_b.StartAdvertising(std::string(kServiceIDA), PowerLevel::kHighPower,
                         Ble::AdvertisingType::kFast, advertisement_bytes);

  EXPECT_TRUE(ble_a.StartScanning(
      std::string(kServiceIDA), Pcp::kP2pPointToPoint, PowerLevel::kHighPower,
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
  ble_b.StopAdvertising(std::string(kServiceIDA));
  EXPECT_TRUE(ble_a.StopScanning(std::string(kServiceIDA)));
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
  ByteArray advertisement_bytes((std::string(kAdvertisementString)));
  CountDownLatch found_latch(1);

  ble_b.StartScanning(
      std::string(kServiceIDA), Pcp::kP2pPointToPoint, PowerLevel::kHighPower,
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

  EXPECT_TRUE(ble_a.StartAdvertising(
      std::string(kServiceIDA), PowerLevel::kHighPower,
      Ble::AdvertisingType::kRegular, advertisement_bytes));
  EXPECT_TRUE(found_latch.Await(kWaitDuration).result());
  EXPECT_TRUE(ble_a.StopAdvertising(std::string(kServiceIDA)));
  ble_b.StopScanning(std::string(kServiceIDA));
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
  ByteArray advertisement_bytes((std::string(kAdvertisementString)));
  CountDownLatch found_latch(1);

  ble_b.StartAdvertising(std::string(kServiceIDA), PowerLevel::kHighPower,
                         Ble::AdvertisingType::kRegular, advertisement_bytes);

  EXPECT_TRUE(ble_a.StartScanning(
      std::string(kServiceIDA), Pcp::kP2pPointToPoint, PowerLevel::kHighPower,
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
  ble_b.StopAdvertising(std::string(kServiceIDA));
  EXPECT_TRUE(ble_a.StopScanning(std::string(kServiceIDA)));
  env_.Stop();
}

TEST_F(BleTest, CanNotStartScanningWhenRadioNotEnabled) {
  env_.Start();
  BluetoothRadio radio_a;
  Ble ble_a(radio_a);
  radio_a.Disable();

  EXPECT_FALSE(ble_a.StartScanning(
      std::string(kServiceIDA), Pcp::kP2pPointToPoint, PowerLevel::kHighPower,
      /*include_dct_advertisement=*/false,
      mediums::DiscoveredPeripheralCallback{}));
  env_.Stop();
}

TEST_F(BleTest, CanNotStartAcceptingConnectionsWhenRadioNotEnabled) {
  env_.Start();
  BluetoothRadio radio_a;
  Ble ble_a(radio_a);
  radio_a.Disable();

  EXPECT_FALSE(ble_a.StartAcceptingConnections(
      std::string(kServiceIDA), [&](BleSocket socket, const std::string&) {}));
  env_.Stop();
}

TEST_F(BleTest, CanStartScanningWithDifferentPowerLevels) {
  env_.Start();
  BluetoothRadio radio_a;
  Ble ble_a(radio_a);
  radio_a.Enable();

  EXPECT_TRUE(ble_a.StartScanning(std::string(kServiceIDA),
                                  Pcp::kP2pPointToPoint, PowerLevel::kLowPower,
                                  /*include_dct_advertisement=*/false,
                                  mediums::DiscoveredPeripheralCallback{}));
  EXPECT_TRUE(ble_a.StopScanning(std::string(kServiceIDA)));
  EXPECT_TRUE(ble_a.StartScanning(std::string(kServiceIDA),
                                  Pcp::kP2pPointToPoint, PowerLevel::kHighPower,
                                  /*include_dct_advertisement=*/false,
                                  mediums::DiscoveredPeripheralCallback{}));
  EXPECT_TRUE(ble_a.StopScanning(std::string(kServiceIDA)));
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

  EXPECT_TRUE(ble.StartScanning(std::string(kServiceIDA), Pcp::kP2pPointToPoint,
                                PowerLevel::kHighPower,
                                /*include_dct_advertisement=*/false,
                                mediums::DiscoveredPeripheralCallback{}));
  EXPECT_TRUE(ble.StartScanning(std::string(kServiceIDB), Pcp::kP2pPointToPoint,
                                PowerLevel::kHighPower,
                                /*include_dct_advertisement=*/false,
                                mediums::DiscoveredPeripheralCallback{}));
  EXPECT_TRUE(ble.StopScanning(std::string(kServiceIDA)));

  EXPECT_TRUE(ble.StartScanning(std::string(kServiceIDA), Pcp::kP2pPointToPoint,
                                PowerLevel::kHighPower,
                                /*include_dct_advertisement=*/false,
                                mediums::DiscoveredPeripheralCallback{}));
  EXPECT_TRUE(ble.StopScanning(std::string(kServiceIDA)));
  EXPECT_TRUE(ble.StopScanning(std::string(kServiceIDB)));
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
  ByteArray advertisement_bytes((std::string(kAdvertisementString)));

  // Device A starts advertising with service IDA and IDB.
  EXPECT_TRUE(
      ble_a.StartAdvertising(std::string(kServiceIDA), PowerLevel::kHighPower,
                             Ble::AdvertisingType::kFast, advertisement_bytes));
  EXPECT_TRUE(
      ble_a.StartAdvertising(std::string(kServiceIDB), PowerLevel::kHighPower,
                             Ble::AdvertisingType::kFast, advertisement_bytes));

  // Device B starts scanning with service IDA and IDB
  EXPECT_TRUE(ble_b.StartScanning(std::string(kServiceIDA),
                                  Pcp::kP2pPointToPoint, PowerLevel::kHighPower,
                                  /*include_dct_advertisement=*/false,
                                  mediums::DiscoveredPeripheralCallback{}));
  EXPECT_TRUE(ble_b.StartScanning(std::string(kServiceIDB),
                                  Pcp::kP2pPointToPoint, PowerLevel::kHighPower,
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
  ByteArray advertisement_bytes((std::string(kAdvertisementString)));
  CountDownLatch found_latch(1);
  CountDownLatch lost_latch(1);

  ble_b.StartAdvertising(std::string(kServiceIDA), PowerLevel::kHighPower,
                         Ble::AdvertisingType::kFast, advertisement_bytes);

  ble_a.StartScanning(
      std::string(kServiceIDA), Pcp::kP2pPointToPoint, PowerLevel::kHighPower,
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

  ble_b.StopAdvertising(std::string(kServiceIDA));

  // Wait for a while (2 times delay) to let the alarm occur twice and
  // `ProcessLostGattAdvertisements` twice to lost periperal.
  SystemClock::Sleep(absl::Milliseconds(kPeripheralLostTimeoutInMillis) * 2);

  EXPECT_TRUE(lost_latch.Await(kWaitDuration).result());

  ble_a.StopScanning(std::string(kServiceIDA));
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
  ByteArray advertisement_bytes((std::string(kAdvertisementString)));
  CountDownLatch found_latch(1);
  CountDownLatch lost_latch(1);

  ble_b.StartAdvertising(std::string(kServiceIDA), PowerLevel::kHighPower,
                         Ble::AdvertisingType::kFast, advertisement_bytes);

  ble_a.StartScanning(
      std::string(kServiceIDA), Pcp::kP2pPointToPoint, PowerLevel::kHighPower,
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

  ble_b.StopAdvertising(std::string(kServiceIDA));
  ble_a.StopScanning(std::string(kServiceIDA));

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
  ByteArray advertisement_bytes((std::string(kAdvertisementString)));
  CountDownLatch found_latch(1);
  CountDownLatch lost_latch(1);

  ble_b.StartAdvertising(std::string(kServiceIDA), PowerLevel::kHighPower,
                         Ble::AdvertisingType::kRegular, advertisement_bytes);

  ble_a.StartScanning(
      std::string(kServiceIDA), Pcp::kP2pPointToPoint, PowerLevel::kHighPower,
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

  ble_b.StopAdvertising(std::string(kServiceIDA));

  // Wait for a while (2 times delay) to let the alarm occur twice and
  // `ProcessLostGattAdvertisements` twice to lost periperal.
  SystemClock::Sleep(absl::Milliseconds(kPeripheralLostTimeoutInMillis) * 2);

  EXPECT_TRUE(lost_latch.Await(kWaitDuration).result());

  ble_a.StopScanning(std::string(kServiceIDA));
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
  ByteArray advertisement_bytes((std::string(kAdvertisementString)));
  CountDownLatch found_latch(1);
  CountDownLatch lost_latch(1);

  ble_b.StartAdvertising(std::string(kServiceIDA), PowerLevel::kHighPower,
                         Ble::AdvertisingType::kRegular, advertisement_bytes);

  ble_a.StartScanning(
      std::string(kServiceIDA), Pcp::kP2pPointToPoint, PowerLevel::kHighPower,
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

  ble_b.StopAdvertising(std::string(kServiceIDA));
  ble_a.StopScanning(std::string(kServiceIDA));

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
  EXPECT_TRUE(
      ble_a.StartLegacyAdvertising(service_id, std::string(kLocalEndpointId),
                                   std::string(kFastAdvertisementServiceUuid)));
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
  EXPECT_FALSE(
      ble_a.StartLegacyAdvertising(service_id, std::string(kLocalEndpointId),
                                   std::string(kFastAdvertisementServiceUuid)));
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
  EXPECT_TRUE(
      ble_a.StartLegacyAdvertising(service_id, std::string(kLocalEndpointId),
                                   std::string(kFastAdvertisementServiceUuid)));
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
  ByteArray advertisement_bytes((std::string(kAdvertisementString)));

  ble_a.StartAdvertising(std::string(kServiceIDA), PowerLevel::kHighPower,
                         Ble::AdvertisingType::kRegular, advertisement_bytes);
  EXPECT_TRUE(ble_a.IsAdvertising(service_id));
  EXPECT_TRUE(
      ble_a.StartLegacyAdvertising(service_id, std::string(kLocalEndpointId),
                                   std::string(kFastAdvertisementServiceUuid)));
  EXPECT_TRUE(ble_a.IsAdvertisingForLegacyDevice(service_id));
  ble_a.StopAdvertising(std::string(kServiceIDA));
  env_.Stop();
}

TEST_F(BleTest, DuplicateStartLegacyAdvertisingReturnsFalse) {
  env_.Start();
  BluetoothRadio radio_a;
  Ble ble_a{radio_a};
  radio_a.Enable();
  std::string service_id(kServiceIDA);
  EXPECT_FALSE(ble_a.IsAdvertisingForLegacyDevice(service_id));
  EXPECT_TRUE(
      ble_a.StartLegacyAdvertising(service_id, std::string(kLocalEndpointId),
                                   std::string(kFastAdvertisementServiceUuid)));
  EXPECT_TRUE(ble_a.IsAdvertisingForLegacyDevice(service_id));
  EXPECT_FALSE(
      ble_a.StartLegacyAdvertising(service_id, std::string(kLocalEndpointId),
                                   std::string(kFastAdvertisementServiceUuid)));
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
  ByteArray advertisement_bytes((std::string(kAdvertisementString)));
  CountDownLatch legacy_device_found_latch(1);

  ble_b.StartLegacyAdvertising(std::string(kServiceIDA),
                               std::string(kLocalEndpointId),
                               std::string(kFastAdvertisementServiceUuid));
  EXPECT_FALSE(ble_a.IsAdvertisingForLegacyDevice(std::string(kServiceIDA)));
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
  ble_b.StopAdvertising(std::string(kServiceIDA));
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
  ByteArray advertisement_bytes((std::string(kAdvertisementString)));
  CountDownLatch found_latch(1);

  ble_b.StartAdvertising(std::string(kServiceIDA), PowerLevel::kHighPower,
                         Ble::AdvertisingType::kRegular, advertisement_bytes);

  EXPECT_TRUE(ble_a.StartScanning(
      std::string(kServiceIDA), Pcp::kP2pPointToPoint, PowerLevel::kHighPower,
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
  ble_b.StopAdvertising(std::string(kServiceIDA));
  EXPECT_TRUE(ble_a.StopScanning(std::string(kServiceIDA)));
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
  ByteArray advertisement_bytes((std::string(kAdvertisementString)));
  CountDownLatch found_latch(1);

  ble_b.StartAdvertising(std::string(kServiceIDA), PowerLevel::kHighPower,
                         Ble::AdvertisingType::kRegular, advertisement_bytes);

  // Disable radio a to simulate platform error.
  radio_a.Disable();
  EXPECT_FALSE(ble_a.StartScanning(
      std::string(kServiceIDA), Pcp::kP2pPointToPoint, PowerLevel::kHighPower,
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
      std::string(kServiceIDA), Pcp::kP2pPointToPoint, PowerLevel::kHighPower,
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
  ble_b.StopAdvertising(std::string(kServiceIDA));

  EXPECT_TRUE(ble_a.StopScanning(std::string(kServiceIDA)));

  // Should return false the second time, as we removed service ID from the map.
  EXPECT_FALSE(ble_a.StopScanning(std::string(kServiceIDA)));
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
  ByteArray advertisement_bytes((std::string(kAdvertisementString)));
  CountDownLatch found_latch(1);
  CountDownLatch lost_latch(1);

  ble_b.StartAdvertising(std::string(kServiceIDA), PowerLevel::kHighPower,
                         Ble::AdvertisingType::kRegular, advertisement_bytes);

  EXPECT_TRUE(ble_a.StartScanning(
      std::string(kServiceIDA), Pcp::kP2pPointToPoint, PowerLevel::kHighPower,
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
  EXPECT_TRUE(ble_b.StopAdvertising(std::string(kServiceIDA)));

  // Wait for a while (2 times delay) to let the alarm occur twice and
  // `ProcessLostGattAdvertisements` twice to lost periperal.
  SystemClock::Sleep(absl::Milliseconds(kPeripheralLostTimeoutInMillis) * 2);

  EXPECT_TRUE(lost_latch.Await(kWaitDuration).result());

  EXPECT_TRUE(ble_a.StopScanning(std::string(kServiceIDA)));
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
  ByteArray advertisement_bytes((std::string(kAdvertisementString)));
  CountDownLatch found_latch(1);
  CountDownLatch lost_latch(1);

  ble_b.StartAdvertising(std::string(kServiceIDA), PowerLevel::kHighPower,
                         Ble::AdvertisingType::kRegular, advertisement_bytes);

  EXPECT_TRUE(ble_a.StartScanning(
      std::string(kServiceIDA), Pcp::kP2pPointToPoint, PowerLevel::kHighPower,
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

  EXPECT_TRUE(ble_b.StopAdvertising(std::string(kServiceIDA)));
  EXPECT_TRUE(ble_a.StopScanning(std::string(kServiceIDA)));

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
  ByteArray advertisement_bytes_a((std::string(kAdvertisementString)));
  ByteArray advertisement_bytes_b((std::string(kAdvertisementStringB)));
  CountDownLatch found_latch_a(1);
  CountDownLatch found_latch_b(1);

  ble_advertiser_a.StartAdvertising(
      std::string(kServiceIDA), PowerLevel::kHighPower,
      Ble::AdvertisingType::kRegular, advertisement_bytes_a);
  ble_advertiser_b.StartAdvertising(
      std::string(kServiceIDB), PowerLevel::kHighPower,
      Ble::AdvertisingType::kRegular, advertisement_bytes_b);

  ble_scanner.StartScanning(
      std::string(kServiceIDA), Pcp::kP2pPointToPoint, PowerLevel::kHighPower,
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
      std::string(kServiceIDB), Pcp::kP2pPointToPoint, PowerLevel::kHighPower,
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
  EXPECT_TRUE(ble_scanner.StopScanning(std::string(kServiceIDA)));
  EXPECT_TRUE(ble_scanner.StopScanning(std::string(kServiceIDB)));
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
  ByteArray advertisement_bytes_a((std::string(kAdvertisementString)));
  ByteArray advertisement_bytes_b((std::string(kAdvertisementStringB)));
  CountDownLatch found_latch_a(1);
  CountDownLatch found_latch_b(1);
  CountDownLatch lost_latch_a(1);
  CountDownLatch lost_latch_b(1);

  ble_advertiser_a.StartAdvertising(
      std::string(kServiceIDA), PowerLevel::kHighPower,
      Ble::AdvertisingType::kRegular, advertisement_bytes_a);
  ble_advertiser_b.StartAdvertising(
      std::string(kServiceIDB), PowerLevel::kHighPower,
      Ble::AdvertisingType::kRegular, advertisement_bytes_b);

  ble_scanner.StartScanning(
      std::string(kServiceIDA), Pcp::kP2pPointToPoint, PowerLevel::kHighPower,
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
      std::string(kServiceIDB), Pcp::kP2pPointToPoint, PowerLevel::kHighPower,
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

  EXPECT_TRUE(ble_advertiser_a.StopAdvertising(std::string(kServiceIDA)));

  // Wait for a while (2 times delay) to let the alarm occur twice and
  // `ProcessLostGattAdvertisements` twice to lost periperal.
  SystemClock::Sleep(absl::Milliseconds(kPeripheralLostTimeoutInMillis) * 2);

  EXPECT_TRUE(lost_latch_a.Await(kWaitDuration).result());

  EXPECT_TRUE(ble_advertiser_b.StopAdvertising(std::string(kServiceIDB)));

  // Wait for a while (2 times delay) to let the alarm occur twice and
  // `ProcessLostGattAdvertisements` twice to lost periperal.
  SystemClock::Sleep(absl::Milliseconds(kPeripheralLostTimeoutInMillis) * 2);

  EXPECT_TRUE(lost_latch_b.Await(kWaitDuration).result());

  EXPECT_TRUE(ble_scanner.StopScanning(std::string(kServiceIDA)));
  EXPECT_TRUE(ble_scanner.StopScanning(std::string(kServiceIDB)));
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

}  // namespace
}  // namespace connections
}  // namespace nearby
