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

#include "connections/implementation/mediums/ble_v2.h"

#include <cstdint>
#include <string>
#include <utility>

#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "connections/implementation/flags/nearby_connections_feature_flags.h"
#include "connections/implementation/mediums/ble_v2/discovered_peripheral_callback.h"
#include "connections/implementation/mediums/bluetooth_radio.h"
#include "connections/power_level.h"
#include "internal/flags/nearby_flags.h"
#include "internal/platform/ble_v2.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/feature_flags.h"
#include "internal/platform/logging.h"
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

class BleV2Test : public testing::TestWithParam<FeatureFlags> {
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

TEST_P(BleV2Test, CanConnect) {
  FeatureFlags feature_flags = GetParam();
  env_.SetFeatureFlags(feature_flags);
  env_.Start();
  BluetoothRadio radio_client;
  BluetoothRadio radio_server;
  BleV2 ble_client{radio_client};
  BleV2 ble_server{radio_server};
  radio_client.Enable();
  radio_server.Enable();
  std::string service_id(kServiceIDA);
  ByteArray advertisement_bytes{std::string(kAdvertisementString)};
  CountDownLatch discovered_latch(1);
  CountDownLatch accept_latch(1);

  BleV2Socket socket_for_server;
  EXPECT_TRUE(ble_server.StartAcceptingConnections(
      service_id, [&](BleV2Socket socket, const std::string&) {
        socket_for_server = std::move(socket);
        accept_latch.CountDown();
      }));

  ble_server.StartAdvertising(service_id, advertisement_bytes,
                              PowerLevel::kHighPower,
                              /*is_fast_advertisement=*/true);

  BleV2Peripheral discovered_peripheral;
  ble_client.StartScanning(
      service_id, PowerLevel::kHighPower,
      mediums::DiscoveredPeripheralCallback{
          .peripheral_discovered_cb =
              [&discovered_latch, &discovered_peripheral](
                  BleV2Peripheral peripheral, const std::string& service_id,
                  const ByteArray& advertisement_bytes,
                  bool fast_advertisement) {
                discovered_peripheral = peripheral;
                NEARBY_LOGS(INFO)
                    << "Discovered peripheral="
                    << peripheral.GetAddress().value_or("")
                    << ", fast advertisement=" << fast_advertisement;
                discovered_latch.CountDown();
              },
      });
  discovered_latch.Await(kWaitDuration).result();
  ASSERT_TRUE(discovered_peripheral.IsValid());

  CancellationFlag flag;
  BleV2Socket socket_for_client =
      ble_client.Connect(service_id, discovered_peripheral, &flag);
  EXPECT_TRUE(accept_latch.Await(kWaitDuration).result());
  EXPECT_TRUE(ble_server.StopAcceptingConnections(service_id));
  EXPECT_TRUE(ble_server.StopAdvertising(service_id));
  EXPECT_TRUE(socket_for_server.IsValid());
  EXPECT_TRUE(socket_for_client.IsValid());
  EXPECT_TRUE(socket_for_server.GetRemotePeripheral().IsValid());
  EXPECT_TRUE(socket_for_client.GetRemotePeripheral().IsValid());
  env_.Stop();
}

TEST_P(BleV2Test, CanCancelConnect) {
  FeatureFlags feature_flags = GetParam();
  env_.SetFeatureFlags(feature_flags);
  env_.Start();
  BluetoothRadio radio_client;
  BluetoothRadio radio_server;
  BleV2 ble_client{radio_client};
  BleV2 ble_server{radio_server};
  radio_client.Enable();
  radio_server.Enable();
  std::string service_id(kServiceIDA);
  ByteArray advertisement_bytes{std::string(kAdvertisementString)};
  CountDownLatch discovered_latch(1);
  CountDownLatch accept_latch(1);

  BleV2Socket socket_for_server;
  EXPECT_TRUE(ble_server.StartAcceptingConnections(
      service_id, [&](BleV2Socket socket, const std::string&) {
        socket_for_server = std::move(socket);
        accept_latch.CountDown();
      }));

  ble_server.StartAdvertising(service_id, advertisement_bytes,
                              PowerLevel::kHighPower,
                              /*is_fast_advertisement=*/true);

  BleV2Peripheral discovered_peripheral;
  ble_client.StartScanning(
      service_id, PowerLevel::kHighPower,
      mediums::DiscoveredPeripheralCallback{
          .peripheral_discovered_cb =
              [&discovered_latch, &discovered_peripheral](
                  BleV2Peripheral peripheral, const std::string& service_id,
                  const ByteArray& advertisement_bytes,
                  bool fast_advertisement) {
                discovered_peripheral = peripheral;
                NEARBY_LOGS(INFO)
                    << "Discovered peripheral="
                    << peripheral.GetAddress().value_or("")
                    << ", fast advertisement=" << fast_advertisement;
                discovered_latch.CountDown();
              },
      });
  EXPECT_TRUE(discovered_latch.Await(kWaitDuration).result());
  ASSERT_TRUE(discovered_peripheral.IsValid());

  CancellationFlag flag(true);
  BleV2Socket socket_for_client =
      ble_client.Connect(service_id, discovered_peripheral, &flag);
  // If FeatureFlag is disabled, Cancelled is false as no-op.
  if (!feature_flags.enable_cancellation_flag) {
    EXPECT_TRUE(accept_latch.Await(kWaitDuration).result());
    EXPECT_TRUE(ble_server.StopAcceptingConnections(service_id));
    EXPECT_TRUE(ble_server.StopAdvertising(service_id));
    EXPECT_TRUE(socket_for_server.IsValid());
    EXPECT_TRUE(socket_for_client.IsValid());
    EXPECT_TRUE(socket_for_server.GetRemotePeripheral().IsValid());
    EXPECT_TRUE(socket_for_client.GetRemotePeripheral().IsValid());
  } else {
    EXPECT_FALSE(accept_latch.Await(kWaitDuration).result());
    EXPECT_TRUE(ble_server.StopAcceptingConnections(service_id));
    EXPECT_TRUE(ble_server.StopAdvertising(service_id));
    EXPECT_FALSE(socket_for_server.IsValid());
    EXPECT_FALSE(socket_for_client.IsValid());
  }
  env_.Stop();
}

INSTANTIATE_TEST_SUITE_P(ParametrisedBleTest, BleV2Test,
                         ::testing::ValuesIn(kTestCases));

TEST_F(BleV2Test, CanConstructValidObject) {
  env_.Start();
  BluetoothRadio radio_a;
  BluetoothRadio radio_b;
  BleV2 ble_a(radio_a);
  BleV2 ble_b(radio_b);

  EXPECT_TRUE(ble_a.IsMediumValid());
  EXPECT_TRUE(ble_a.IsAvailable());
  EXPECT_TRUE(ble_b.IsMediumValid());
  EXPECT_TRUE(ble_b.IsAvailable());
  EXPECT_NE(&radio_a.GetBluetoothAdapter(), &radio_b.GetBluetoothAdapter());
  env_.Stop();
}

TEST_F(BleV2Test, CanStartFastAdvertising) {
  env_.Start();
  BluetoothRadio radio_a;
  BluetoothRadio radio_b;
  BleV2 ble_a(radio_a);
  BleV2 ble_b(radio_b);
  radio_a.Enable();
  radio_b.Enable();
  ByteArray advertisement_bytes((std::string(kAdvertisementString)));
  CountDownLatch found_latch(1);

  ble_b.StartScanning(
      std::string(kServiceIDA), PowerLevel::kHighPower,
      mediums::DiscoveredPeripheralCallback{
          .peripheral_discovered_cb =
              [&found_latch](BleV2Peripheral peripheral,
                             const std::string& service_id,
                             const ByteArray& advertisement_bytes,
                             bool fast_advertisement) {
                EXPECT_TRUE(fast_advertisement);
                found_latch.CountDown();
              },
      });

  EXPECT_TRUE(ble_a.StartAdvertising(
      std::string(kServiceIDA), advertisement_bytes, PowerLevel::kHighPower,
      /*is_fast_advertisement=*/true));
  EXPECT_TRUE(found_latch.Await(kWaitDuration).result());
  EXPECT_TRUE(ble_a.StopAdvertising(std::string(kServiceIDA)));
  ble_b.StopScanning(std::string(kServiceIDA));
  env_.Stop();
}

TEST_F(BleV2Test, CanStartFastScanning) {
  env_.Start();
  BluetoothRadio radio_a;
  BluetoothRadio radio_b;
  BleV2 ble_a(radio_a);
  BleV2 ble_b(radio_b);
  radio_a.Enable();
  radio_b.Enable();
  ByteArray advertisement_bytes((std::string(kAdvertisementString)));
  CountDownLatch found_latch(1);

  ble_b.StartAdvertising(std::string(kServiceIDA), advertisement_bytes,
                         PowerLevel::kHighPower,
                         /*is_fast_advertisement=*/true);

  EXPECT_TRUE(ble_a.StartScanning(
      std::string(kServiceIDA), PowerLevel::kHighPower,
      mediums::DiscoveredPeripheralCallback{
          .peripheral_discovered_cb =
              [&found_latch](BleV2Peripheral peripheral,
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

TEST_F(BleV2Test, CanStartAdvertising) {
  env_.Start();
  BluetoothRadio radio_a;
  BluetoothRadio radio_b;
  BleV2 ble_a(radio_a);
  BleV2 ble_b(radio_b);
  radio_a.Enable();
  radio_b.Enable();
  ByteArray advertisement_bytes((std::string(kAdvertisementString)));
  CountDownLatch found_latch(1);

  ble_b.StartScanning(
      std::string(kServiceIDA), PowerLevel::kHighPower,
      mediums::DiscoveredPeripheralCallback{
          .peripheral_discovered_cb =
              [&found_latch](BleV2Peripheral peripheral,
                             const std::string& service_id,
                             const ByteArray& advertisement_bytes,
                             bool fast_advertisement) {
                EXPECT_FALSE(fast_advertisement);
                found_latch.CountDown();
              },
      });

  EXPECT_TRUE(ble_a.StartAdvertising(
      std::string(kServiceIDA), advertisement_bytes, PowerLevel::kHighPower,
      /*is_fast_advertisement=*/false));
  EXPECT_TRUE(found_latch.Await(kWaitDuration).result());
  EXPECT_TRUE(ble_a.StopAdvertising(std::string(kServiceIDA)));
  ble_b.StopScanning(std::string(kServiceIDA));
  env_.Stop();
}

TEST_F(BleV2Test, CanStartScanning) {
  env_.Start();
  BluetoothRadio radio_a;
  BluetoothRadio radio_b;
  BleV2 ble_a(radio_a);
  BleV2 ble_b(radio_b);
  radio_a.Enable();
  radio_b.Enable();
  ByteArray advertisement_bytes((std::string(kAdvertisementString)));
  CountDownLatch found_latch(1);

  ble_b.StartAdvertising(std::string(kServiceIDA), advertisement_bytes,
                         PowerLevel::kHighPower,
                         /*is_fast_advertisement=*/false);

  EXPECT_TRUE(ble_a.StartScanning(
      std::string(kServiceIDA), PowerLevel::kHighPower,
      mediums::DiscoveredPeripheralCallback{
          .peripheral_discovered_cb =
              [&found_latch](BleV2Peripheral peripheral,
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

TEST_F(BleV2Test, CanStartStopMultipleScanningWithDifferentServiceIds) {
  env_.Start();
  BluetoothRadio radio;
  BleV2 ble(radio);
  radio.Enable();

  EXPECT_TRUE(ble.StartScanning(std::string(kServiceIDA),
                                PowerLevel::kHighPower,
                                mediums::DiscoveredPeripheralCallback{}));
  EXPECT_TRUE(ble.StartScanning(std::string(kServiceIDB),
                                PowerLevel::kHighPower,
                                mediums::DiscoveredPeripheralCallback{}));
  EXPECT_TRUE(ble.StopScanning(std::string(kServiceIDA)));

  EXPECT_TRUE(ble.StartScanning(std::string(kServiceIDA),
                                PowerLevel::kHighPower,
                                mediums::DiscoveredPeripheralCallback{}));
  EXPECT_TRUE(ble.StopScanning(std::string(kServiceIDA)));
  EXPECT_TRUE(ble.StopScanning(std::string(kServiceIDB)));
  env_.Stop();
}

TEST_F(BleV2Test, DestructWorksForStartAdvertisingAndScanningWithoutStop) {
  env_.Start();
  BluetoothRadio radio_a;
  BluetoothRadio radio_b;
  BleV2 ble_a(radio_a);
  BleV2 ble_b(radio_b);

  radio_a.Enable();
  radio_b.Enable();
  ByteArray advertisement_bytes((std::string(kAdvertisementString)));

  // Device A starts advertising with service IDA and IDB.
  EXPECT_TRUE(ble_a.StartAdvertising(
      std::string(kServiceIDA), advertisement_bytes, PowerLevel::kHighPower,
      /*is_fast_advertisement=*/true));
  EXPECT_TRUE(ble_a.StartAdvertising(
      std::string(kServiceIDB), advertisement_bytes, PowerLevel::kHighPower,
      /*is_fast_advertisement=*/true));

  // Device B starts scanning with service IDA and IDB
  EXPECT_TRUE(ble_b.StartScanning(std::string(kServiceIDA),
                                  PowerLevel::kHighPower,
                                  mediums::DiscoveredPeripheralCallback{}));
  EXPECT_TRUE(ble_b.StartScanning(std::string(kServiceIDB),
                                  PowerLevel::kHighPower,
                                  mediums::DiscoveredPeripheralCallback{}));
  env_.Stop();
}

TEST_F(BleV2Test, StartFastScanningDiscoverAndLostPeripheral) {
  env_.Start();
  BluetoothRadio radio_a;
  BluetoothRadio radio_b;
  BleV2 ble_a(radio_a);
  BleV2 ble_b(radio_b);
  radio_a.Enable();
  radio_b.Enable();
  ByteArray advertisement_bytes((std::string(kAdvertisementString)));
  CountDownLatch found_latch(1);
  CountDownLatch lost_latch(1);

  ble_b.StartAdvertising(std::string(kServiceIDA), advertisement_bytes,
                         PowerLevel::kHighPower,
                         /*is_fast_advertisement=*/true);

  ble_a.StartScanning(
      std::string(kServiceIDA), PowerLevel::kHighPower,
      mediums::DiscoveredPeripheralCallback{
          .peripheral_discovered_cb =
              [&found_latch](BleV2Peripheral peripheral,
                             const std::string& service_id,
                             const ByteArray& advertisement_bytes,
                             bool fast_advertisement) {
                EXPECT_TRUE(fast_advertisement);
                found_latch.CountDown();
              },
          .peripheral_lost_cb =
              [&lost_latch](
                  BleV2Peripheral peripheral, const std::string& service_id,
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

TEST_F(BleV2Test,
       StartFastScanningDiscoverButNoPeripheralLostAfterStopScanning) {
  env_.Start();
  BluetoothRadio radio_a;
  BluetoothRadio radio_b;
  BleV2 ble_a(radio_a);
  BleV2 ble_b(radio_b);
  radio_a.Enable();
  radio_b.Enable();
  ByteArray advertisement_bytes((std::string(kAdvertisementString)));
  CountDownLatch found_latch(1);
  CountDownLatch lost_latch(1);

  ble_b.StartAdvertising(std::string(kServiceIDA), advertisement_bytes,
                         PowerLevel::kHighPower,
                         /*is_fast_advertisement=*/true);

  ble_a.StartScanning(
      std::string(kServiceIDA), PowerLevel::kHighPower,
      mediums::DiscoveredPeripheralCallback{
          .peripheral_discovered_cb =
              [&found_latch](BleV2Peripheral peripheral,
                             const std::string& service_id,
                             const ByteArray& advertisement_bytes,
                             bool fast_advertisement) {
                EXPECT_TRUE(fast_advertisement);
                found_latch.CountDown();
              },
          .peripheral_lost_cb =
              [&lost_latch](
                  BleV2Peripheral peripheral, const std::string& service_id,
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

TEST_F(BleV2Test, StartScanningDiscoverAndLostPeripheral) {
  env_.Start();
  BluetoothRadio radio_a;
  BluetoothRadio radio_b;
  BleV2 ble_a(radio_a);
  BleV2 ble_b(radio_b);
  radio_a.Enable();
  radio_b.Enable();
  ByteArray advertisement_bytes((std::string(kAdvertisementString)));
  CountDownLatch found_latch(1);
  CountDownLatch lost_latch(1);

  ble_b.StartAdvertising(std::string(kServiceIDA), advertisement_bytes,
                         PowerLevel::kHighPower,
                         /*is_fast_advertisement=*/false);

  ble_a.StartScanning(
      std::string(kServiceIDA), PowerLevel::kHighPower,
      mediums::DiscoveredPeripheralCallback{
          .peripheral_discovered_cb =
              [&found_latch](BleV2Peripheral peripheral,
                             const std::string& service_id,
                             const ByteArray& advertisement_bytes,
                             bool fast_advertisement) {
                EXPECT_FALSE(fast_advertisement);
                found_latch.CountDown();
              },
          .peripheral_lost_cb =
              [&lost_latch](
                  BleV2Peripheral peripheral, const std::string& service_id,
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

TEST_F(BleV2Test, StartScanningDiscoverButNoPeripheralLostAfterStopScanning) {
  env_.Start();
  BluetoothRadio radio_a;
  BluetoothRadio radio_b;
  BleV2 ble_a(radio_a);
  BleV2 ble_b(radio_b);
  radio_a.Enable();
  radio_b.Enable();
  ByteArray advertisement_bytes((std::string(kAdvertisementString)));
  CountDownLatch found_latch(1);
  CountDownLatch lost_latch(1);

  ble_b.StartAdvertising(std::string(kServiceIDA), advertisement_bytes,
                         PowerLevel::kHighPower,
                         /*is_fast_advertisement=*/false);

  ble_a.StartScanning(
      std::string(kServiceIDA), PowerLevel::kHighPower,
      mediums::DiscoveredPeripheralCallback{
          .peripheral_discovered_cb =
              [&found_latch](BleV2Peripheral peripheral,
                             const std::string& service_id,
                             const ByteArray& advertisement_bytes,
                             bool fast_advertisement) {
                EXPECT_FALSE(fast_advertisement);
                found_latch.CountDown();
              },
          .peripheral_lost_cb =
              [&lost_latch](
                  BleV2Peripheral peripheral, const std::string& service_id,
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

TEST_F(BleV2Test, CanStartAndStopLegacyAdvertising) {
  env_.Start();
  BluetoothRadio radio_a;
  BleV2 ble_a{radio_a};
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

TEST_F(BleV2Test, CanNotStartLegacyAdvertisingWhenRadioNotEnabled) {
  env_.Start();
  BluetoothRadio radio_a;
  BleV2 ble_a{radio_a};
  radio_a.Disable();
  std::string service_id(kServiceIDA);
  EXPECT_FALSE(
      ble_a.StartLegacyAdvertising(service_id, std::string(kLocalEndpointId),
                                   std::string(kFastAdvertisementServiceUuid)));
  EXPECT_FALSE(ble_a.IsAdvertisingForLegacyDevice(service_id));
  env_.Stop();
}

TEST_F(BleV2Test, CanNotStopLegacyAdvertisingForNonExistingServiceId) {
  env_.Start();
  BluetoothRadio radio_a;
  BleV2 ble_a{radio_a};
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

TEST_F(BleV2Test, StartLegacyAdvertisingNotBlockedByRegularAdvertising) {
  env_.Start();
  BluetoothRadio radio_a;
  BleV2 ble_a{radio_a};
  radio_a.Enable();
  std::string service_id(kServiceIDA);
  ByteArray advertisement_bytes((std::string(kAdvertisementString)));

  ble_a.StartAdvertising(std::string(kServiceIDA), advertisement_bytes,
                         PowerLevel::kHighPower,
                         /*is_fast_advertisement=*/false);
  EXPECT_TRUE(ble_a.IsAdvertising(service_id));
  EXPECT_TRUE(
      ble_a.StartLegacyAdvertising(service_id, std::string(kLocalEndpointId),
                                   std::string(kFastAdvertisementServiceUuid)));
  EXPECT_TRUE(ble_a.IsAdvertisingForLegacyDevice(service_id));
  ble_a.StopAdvertising(std::string(kServiceIDA));
  env_.Stop();
}

TEST_F(BleV2Test, DuplicateStartLegacyAdvertisingReturnsFalse) {
  env_.Start();
  BluetoothRadio radio_a;
  BleV2 ble_a{radio_a};
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

TEST_F(BleV2Test, HandleLegacyAdvertising) {
  env_.SetFeatureFlags(
      {FeatureFlags{.enable_invoking_legacy_device_discovered_cb = true}});
  env_.Start();
  BluetoothRadio radio_a;
  BluetoothRadio radio_b;
  BleV2 ble_a(radio_a);
  BleV2 ble_b(radio_b);
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
      legacy_service_id, PowerLevel::kHighPower,
      mediums::DiscoveredPeripheralCallback{
          .peripheral_discovered_cb =
              [](BleV2Peripheral peripheral, const std::string& service_id,
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

TEST_F(BleV2Test, CanStartAsyncScanning) {
  env_.SetFeatureFlags({FeatureFlags{.enable_ble_v2_async_scanning = true}});
  env_.Start();
  BluetoothRadio radio_a;
  BluetoothRadio radio_b;
  BleV2 ble_a(radio_a);
  BleV2 ble_b(radio_b);
  radio_a.Enable();
  radio_b.Enable();
  ByteArray advertisement_bytes((std::string(kAdvertisementString)));
  CountDownLatch found_latch(1);

  ble_b.StartAdvertising(std::string(kServiceIDA), advertisement_bytes,
                         PowerLevel::kHighPower,
                         /*is_fast_advertisement=*/false);

  EXPECT_TRUE(ble_a.StartScanning(
      std::string(kServiceIDA), PowerLevel::kHighPower,
      mediums::DiscoveredPeripheralCallback{
          .peripheral_discovered_cb =
              [&found_latch](BleV2Peripheral peripheral,
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

TEST_F(BleV2Test, StartAsyncScanningWithPlatformErrors) {
  env_.SetFeatureFlags({FeatureFlags{.enable_ble_v2_async_scanning = true}});
  env_.Start();
  BluetoothRadio radio_a;
  BluetoothRadio radio_b;
  BleV2 ble_a(radio_a);
  BleV2 ble_b(radio_b);
  radio_a.Enable();
  radio_b.Enable();
  ByteArray advertisement_bytes((std::string(kAdvertisementString)));
  CountDownLatch found_latch(1);

  ble_b.StartAdvertising(std::string(kServiceIDA), advertisement_bytes,
                         PowerLevel::kHighPower,
                         /*is_fast_advertisement=*/false);

  // Disable radio a to simulate platform error.
  radio_a.Disable();
  EXPECT_FALSE(ble_a.StartScanning(
      std::string(kServiceIDA), PowerLevel::kHighPower,
      mediums::DiscoveredPeripheralCallback{
          .peripheral_discovered_cb =
              [&found_latch](BleV2Peripheral peripheral,
                             const std::string& service_id,
                             const ByteArray& advertisement_bytes,
                             bool fast_advertisement) {
                EXPECT_FALSE(fast_advertisement);
                found_latch.CountDown();
              },
      }));

  radio_a.Enable();
  EXPECT_TRUE(ble_a.StartScanning(
      std::string(kServiceIDA), PowerLevel::kHighPower,
      mediums::DiscoveredPeripheralCallback{
          .peripheral_discovered_cb =
              [&found_latch](BleV2Peripheral peripheral,
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

TEST_F(BleV2Test, StartAsyncScanningDiscoverAndLostPeripheral) {
  env_.SetFeatureFlags({FeatureFlags{.enable_ble_v2_async_scanning = true}});
  env_.Start();
  BluetoothRadio radio_a;
  BluetoothRadio radio_b;
  BleV2 ble_a(radio_a);
  BleV2 ble_b(radio_b);
  radio_a.Enable();
  radio_b.Enable();
  ByteArray advertisement_bytes((std::string(kAdvertisementString)));
  CountDownLatch found_latch(1);
  CountDownLatch lost_latch(1);

  ble_b.StartAdvertising(std::string(kServiceIDA), advertisement_bytes,
                         PowerLevel::kHighPower,
                         /*is_fast_advertisement=*/false);

  EXPECT_TRUE(ble_a.StartScanning(
      std::string(kServiceIDA), PowerLevel::kHighPower,
      mediums::DiscoveredPeripheralCallback{
          .peripheral_discovered_cb =
              [&found_latch](BleV2Peripheral peripheral,
                             const std::string& service_id,
                             const ByteArray& advertisement_bytes,
                             bool fast_advertisement) {
                EXPECT_FALSE(fast_advertisement);
                found_latch.CountDown();
              },
          .peripheral_lost_cb =
              [&lost_latch](
                  BleV2Peripheral peripheral, const std::string& service_id,
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

TEST_F(BleV2Test,
       StartAsyncScanningDiscoverButNoPeripheralLostAfterStopScanning) {
  env_.SetFeatureFlags({FeatureFlags{.enable_ble_v2_async_scanning = true}});
  env_.Start();
  BluetoothRadio radio_a;
  BluetoothRadio radio_b;
  BleV2 ble_a(radio_a);
  BleV2 ble_b(radio_b);
  radio_a.Enable();
  radio_b.Enable();
  ByteArray advertisement_bytes((std::string(kAdvertisementString)));
  CountDownLatch found_latch(1);
  CountDownLatch lost_latch(1);

  ble_b.StartAdvertising(std::string(kServiceIDA), advertisement_bytes,
                         PowerLevel::kHighPower,
                         /*is_fast_advertisement=*/false);

  EXPECT_TRUE(ble_a.StartScanning(
      std::string(kServiceIDA), PowerLevel::kHighPower,
      mediums::DiscoveredPeripheralCallback{
          .peripheral_discovered_cb =
              [&found_latch](BleV2Peripheral peripheral,
                             const std::string& service_id,
                             const ByteArray& advertisement_bytes,
                             bool fast_advertisement) {
                EXPECT_FALSE(fast_advertisement);
                found_latch.CountDown();
              },
          .peripheral_lost_cb =
              [&lost_latch](
                  BleV2Peripheral peripheral, const std::string& service_id,
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

TEST_F(BleV2Test, CanStartStopMultipleAsyncScanningWithDifferentServiceIds) {
  env_.SetFeatureFlags({FeatureFlags{.enable_ble_v2_async_scanning = true}});
  env_.Start();
  BluetoothRadio radio_scanner;
  BluetoothRadio radio_advertiser_a;
  BluetoothRadio radio_advertiser_b;
  BleV2 ble_scanner(radio_scanner);
  BleV2 ble_advertiser_a(radio_advertiser_a);
  BleV2 ble_advertiser_b(radio_advertiser_b);
  radio_scanner.Enable();
  radio_advertiser_a.Enable();
  radio_advertiser_b.Enable();
  ByteArray advertisement_bytes_a((std::string(kAdvertisementString)));
  ByteArray advertisement_bytes_b((std::string(kAdvertisementStringB)));
  CountDownLatch found_latch_a(1);
  CountDownLatch found_latch_b(1);

  ble_advertiser_a.StartAdvertising(
      std::string(kServiceIDA), advertisement_bytes_a, PowerLevel::kHighPower,
      /*is_fast_advertisement=*/false);
  ble_advertiser_b.StartAdvertising(
      std::string(kServiceIDB), advertisement_bytes_b, PowerLevel::kHighPower,
      /*is_fast_advertisement=*/false);

  ble_scanner.StartScanning(
      std::string(kServiceIDA), PowerLevel::kHighPower,
      mediums::DiscoveredPeripheralCallback{
          .peripheral_discovered_cb =
              [&found_latch_a](BleV2Peripheral peripheral,
                               const std::string& service_id,
                               const ByteArray& advertisement_bytes,
                               bool fast_advertisement) {
                EXPECT_EQ(service_id, kServiceIDA);
                EXPECT_FALSE(fast_advertisement);
                found_latch_a.CountDown();
              },
      });

  ble_scanner.StartScanning(
      std::string(kServiceIDB), PowerLevel::kHighPower,
      mediums::DiscoveredPeripheralCallback{
          .peripheral_discovered_cb =
              [&found_latch_b](BleV2Peripheral peripheral,
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

TEST_F(BleV2Test, StartMultipleAsyncScanningDiscoverAndLostPeripheral) {
  env_.SetFeatureFlags({FeatureFlags{.enable_ble_v2_async_scanning = true}});
  env_.Start();
  BluetoothRadio radio_scanner;
  BluetoothRadio radio_advertiser_a;
  BluetoothRadio radio_advertiser_b;
  BleV2 ble_scanner(radio_scanner);
  BleV2 ble_advertiser_a(radio_advertiser_a);
  BleV2 ble_advertiser_b(radio_advertiser_b);
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
      std::string(kServiceIDA), advertisement_bytes_a, PowerLevel::kHighPower,
      /*is_fast_advertisement=*/false);
  ble_advertiser_b.StartAdvertising(
      std::string(kServiceIDB), advertisement_bytes_b, PowerLevel::kHighPower,
      /*is_fast_advertisement=*/false);

  ble_scanner.StartScanning(
      std::string(kServiceIDA), PowerLevel::kHighPower,
      mediums::DiscoveredPeripheralCallback{
          .peripheral_discovered_cb =
              [&found_latch_a](BleV2Peripheral peripheral,
                               const std::string& service_id,
                               const ByteArray& advertisement_bytes,
                               bool fast_advertisement) {
                EXPECT_EQ(service_id, kServiceIDA);
                EXPECT_FALSE(fast_advertisement);
                found_latch_a.CountDown();
              },
          .peripheral_lost_cb =
              [&lost_latch_a](BleV2Peripheral peripheral,
                              const std::string& service_id,
                              const ByteArray& advertisement_bytes,
                              bool fast_advertisement) {
                EXPECT_EQ(service_id, kServiceIDA);
                EXPECT_FALSE(fast_advertisement);
                lost_latch_a.CountDown();
              },
      });

  ble_scanner.StartScanning(
      std::string(kServiceIDB), PowerLevel::kHighPower,
      mediums::DiscoveredPeripheralCallback{
          .peripheral_discovered_cb =
              [&found_latch_b](BleV2Peripheral peripheral,
                               const std::string& service_id,
                               const ByteArray& advertisement_bytes,
                               bool fast_advertisement) {
                EXPECT_EQ(service_id, kServiceIDB);
                EXPECT_FALSE(fast_advertisement);
                found_latch_b.CountDown();
              },
          .peripheral_lost_cb =
              [&lost_latch_b](BleV2Peripheral peripheral,
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

}  // namespace
}  // namespace connections
}  // namespace nearby
