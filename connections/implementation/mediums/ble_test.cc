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

#include "connections/implementation/mediums/ble.h"

#include <atomic>
#include <string>

#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "connections/implementation/mediums/bluetooth_radio.h"
#include "internal/platform/ble.h"
#include "internal/platform/bluetooth_adapter.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/expected.h"
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
constexpr absl::string_view kServiceID{"com.google.location.nearby.apps.test"};
constexpr absl::string_view kAdvertisementString{"\x0a\x0b\x0c\x0d"};
constexpr absl::string_view kFastAdvertisementServiceUuid{"\xf3\xfe"};

class BleTest : public ::testing::TestWithParam<FeatureFlags> {
 protected:
  using DiscoveredPeripheralCallback = BleMedium::DiscoveredPeripheralCallback;

  MediumEnvironment& env_{MediumEnvironment::Instance()};
};

TEST_P(BleTest, CanStartAcceptingConnectionsAndConnect) {
  FeatureFlags feature_flags = GetParam();
  env_.SetFeatureFlags(feature_flags);
  env_.Start();
  BluetoothRadio radio_a;
  BluetoothRadio radio_b;
  Ble ble_a{radio_a};
  Ble ble_b{radio_b};
  radio_a.Enable();
  radio_b.Enable();
  std::string service_id(kServiceID);
  ByteArray advertisement_bytes{std::string(kAdvertisementString)};
  std::string fast_advertisement_service_uuid(kFastAdvertisementServiceUuid);
  CountDownLatch found_latch(1);
  CountDownLatch accept_latch(1);

  ble_a.StartAdvertising(service_id, advertisement_bytes,
                         fast_advertisement_service_uuid);
  ble_a.StartAcceptingConnections(
      service_id,
      [&](BleSocket socket, const std::string&) { accept_latch.CountDown(); });
  std::atomic<BlePeripheral> atomic_discovered_peripheral;
  ble_b.StartScanning(
      service_id, fast_advertisement_service_uuid,
      {
          .peripheral_discovered_cb =
              [&found_latch, &atomic_discovered_peripheral](
                  BlePeripheral& peripheral, const std::string& service_id,
                  const ByteArray& advertisement_bytes,
                  bool fast_advertisement) {
                LOG(INFO) << "Discovered peripheral=" << peripheral.GetName()
                          << ", impl=" << &peripheral.GetImpl()
                          << ", fast advertisement=" << fast_advertisement;
                atomic_discovered_peripheral.store(peripheral);
                found_latch.CountDown();
              },
      });

  EXPECT_TRUE(found_latch.Await(kWaitDuration).result());
  BlePeripheral discovered_peripheral = atomic_discovered_peripheral.load();
  ASSERT_TRUE(discovered_peripheral.IsValid());
  CancellationFlag flag;
  ErrorOr<BleSocket> socket_result =
      ble_b.Connect(discovered_peripheral, service_id, &flag);
  EXPECT_TRUE(accept_latch.Await(kWaitDuration).result());
  EXPECT_TRUE(socket_result.has_value());
  EXPECT_TRUE(socket_result.value().IsValid());
  ble_b.StopScanning(service_id);
  ble_a.StopAdvertising(service_id);
  env_.Stop();
}

TEST_P(BleTest, CanCancelConnect) {
  FeatureFlags feature_flags = GetParam();
  env_.SetFeatureFlags(feature_flags);
  env_.Start();
  BluetoothRadio radio_a;
  BluetoothRadio radio_b;
  Ble ble_a{radio_a};
  Ble ble_b{radio_b};
  radio_a.Enable();
  radio_b.Enable();
  std::string service_id(kServiceID);
  ByteArray advertisement_bytes{std::string(kAdvertisementString)};
  std::string fast_advertisement_service_uuid(kFastAdvertisementServiceUuid);
  CountDownLatch found_latch(1);
  CountDownLatch accept_latch(1);

  ble_a.StartAdvertising(service_id, advertisement_bytes,
                         fast_advertisement_service_uuid);
  ble_a.StartAcceptingConnections(
      service_id,
      [&](BleSocket socket, const std::string&) { accept_latch.CountDown(); });
  std::atomic<BlePeripheral> atomic_discovered_peripheral;
  ble_b.StartScanning(
      service_id, fast_advertisement_service_uuid,
      {
          .peripheral_discovered_cb =
              [&found_latch, &atomic_discovered_peripheral](
                  BlePeripheral& peripheral, const std::string& service_id,
                  const ByteArray& advertisement_bytes,
                  bool fast_advertisement) {
                LOG(INFO) << "Discovered peripheral=" << peripheral.GetName()
                          << ", impl=" << &peripheral.GetImpl()
                          << ", fast advertisement=" << fast_advertisement;
                atomic_discovered_peripheral.store(peripheral);
                found_latch.CountDown();
              },
      });

  EXPECT_TRUE(found_latch.Await(kWaitDuration).result());
  BlePeripheral discovered_peripheral = atomic_discovered_peripheral.load();
  ASSERT_TRUE(discovered_peripheral.IsValid());
  CancellationFlag flag(true);
  ErrorOr<BleSocket> socket_result =
      ble_b.Connect(discovered_peripheral, service_id, &flag);
  // If FeatureFlag is disabled, Cancelled is false as no-op.
  if (!feature_flags.enable_cancellation_flag) {
    EXPECT_TRUE(accept_latch.Await(kWaitDuration).result());
    EXPECT_TRUE(socket_result.has_value());
    EXPECT_TRUE(socket_result.value().IsValid());
  } else {
    EXPECT_FALSE(accept_latch.Await(kWaitDuration).result());
    EXPECT_TRUE(socket_result.has_error());
  }
  ble_b.StopScanning(service_id);
  ble_a.StopAdvertising(service_id);
  env_.Stop();
}

INSTANTIATE_TEST_SUITE_P(ParametrisedBleTest, BleTest,
                         ::testing::ValuesIn(kTestCases));

TEST_F(BleTest, CanConstructValidObject) {
  env_.Start();
  BluetoothRadio radio_a;
  BluetoothRadio radio_b;
  Ble ble_a{radio_a};
  Ble ble_b{radio_b};

  EXPECT_TRUE(ble_a.IsMediumValid());
  EXPECT_TRUE(ble_a.IsAdapterValid());
  EXPECT_TRUE(ble_a.IsAvailable());
  EXPECT_TRUE(ble_b.IsMediumValid());
  EXPECT_TRUE(ble_b.IsAdapterValid());
  EXPECT_TRUE(ble_b.IsAvailable());
  EXPECT_NE(&radio_a.GetBluetoothAdapter(), &radio_b.GetBluetoothAdapter());
  env_.Stop();
}

TEST_F(BleTest, CanStartAdvertising) {
  env_.Start();
  BluetoothRadio radio_a;
  BluetoothRadio radio_b;
  Ble ble_a{radio_a};
  Ble ble_b{radio_b};
  radio_a.Enable();
  radio_b.Enable();
  std::string service_id(kServiceID);
  ByteArray advertisement_bytes{std::string(kAdvertisementString)};
  std::string fast_advertisement_service_uuid(kFastAdvertisementServiceUuid);
  CountDownLatch found_latch(1);

  ble_b.StartScanning(
      service_id, fast_advertisement_service_uuid,
      DiscoveredPeripheralCallback{
          .peripheral_discovered_cb =
              [&found_latch](
                  BlePeripheral& peripheral, const std::string& service_id,
                  const ByteArray& advertisement_bytes,
                  bool fast_advertisement) { found_latch.CountDown(); },
      });

  EXPECT_TRUE(ble_a.StartAdvertising(service_id, advertisement_bytes,
                                     fast_advertisement_service_uuid));
  EXPECT_TRUE(found_latch.Await(kWaitDuration).result());
  EXPECT_TRUE(ble_a.StopAdvertising(service_id));
  EXPECT_TRUE(ble_b.StopScanning(service_id));
  env_.Stop();
}

TEST_F(BleTest, CanStartDiscovery) {
  env_.Start();
  BluetoothRadio radio_a;
  BluetoothRadio radio_b;
  Ble ble_a{radio_a};
  Ble ble_b{radio_b};
  radio_a.Enable();
  radio_b.Enable();
  std::string service_id(kServiceID);
  ByteArray advertisement_bytes{std::string(kAdvertisementString)};
  std::string fast_advertisement_service_uuid(kFastAdvertisementServiceUuid);
  CountDownLatch accept_latch(1);
  CountDownLatch lost_latch(1);

  ble_b.StartAdvertising(service_id, advertisement_bytes,
                         fast_advertisement_service_uuid);

  EXPECT_TRUE(ble_a.StartScanning(
      service_id, fast_advertisement_service_uuid,
      DiscoveredPeripheralCallback{
          .peripheral_discovered_cb =
              [&accept_latch](
                  BlePeripheral& peripheral, const std::string& service_id,
                  const ByteArray& advertisement_bytes,
                  bool fast_advertisement) { accept_latch.CountDown(); },
          .peripheral_lost_cb =
              [&lost_latch](BlePeripheral& peripheral,
                            const std::string& service_id) {
                lost_latch.CountDown();
              },
      }));
  EXPECT_TRUE(accept_latch.Await(kWaitDuration).result());
  ble_b.StopAdvertising(service_id);
  EXPECT_TRUE(lost_latch.Await(kWaitDuration).result());
  EXPECT_TRUE(ble_a.StopScanning(service_id));
  env_.Stop();
}

TEST_F(BleTest, CanStartAndStopLegacyAdvertising) {
  env_.Start();
  BluetoothRadio radio_a;
  Ble ble_a{radio_a};
  radio_a.Enable();
  std::string service_id(kServiceID);
  std::string legacy_service_id(std::string{kServiceID} + "-Legacy");
  std::string device_a_endpoint_id{"1A1A"};
  std::string fast_advertisement_service_uuid(kFastAdvertisementServiceUuid);
  EXPECT_TRUE(ble_a.StartLegacyAdvertising(service_id, device_a_endpoint_id,
                                           fast_advertisement_service_uuid));
  EXPECT_FALSE(ble_a.IsAdvertising(service_id));
  EXPECT_TRUE(ble_a.IsAdvertising(legacy_service_id));
  EXPECT_TRUE(ble_a.StopLegacyAdvertising(service_id));
  EXPECT_FALSE(ble_a.IsAdvertising(legacy_service_id));
  env_.Stop();
}

}  // namespace
}  // namespace connections
}  // namespace nearby
