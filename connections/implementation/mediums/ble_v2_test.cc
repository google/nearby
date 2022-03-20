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

#include <string>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "connections/implementation/mediums/ble_v2/discovered_peripheral_callback.h"
#include "connections/implementation/mediums/bluetooth_radio.h"
#include "internal/platform/ble.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/logging.h"
#include "internal/platform/medium_environment.h"

namespace location {
namespace nearby {
namespace connections {
namespace {

constexpr absl::Duration kWaitDuration = absl::Milliseconds(1000);
constexpr absl::string_view kServiceIDA{
    "com.google.location.nearby.apps.test.a"};
constexpr absl::string_view kServiceIDB{
    "com.google.location.nearby.apps.test.b"};
constexpr absl::string_view kAdvertisementString{"\x0a\x0b\x0c\x0d"};
constexpr absl::string_view kFastAdvertisementServiceUuid{"FAST"};

class BleV2Test : public testing::Test {
 protected:
  BleV2Test() { env_.Stop(); }

  MediumEnvironment& env_{MediumEnvironment::Instance()};
};

TEST_F(BleV2Test, CanConstructValidObject) {
  env_.Start();
  BluetoothRadio radio_a;
  BluetoothRadio radio_b;
  BleV2 ble_a{radio_a};
  BleV2 ble_b{radio_b};

  EXPECT_TRUE(ble_a.IsMediumValid());
  EXPECT_TRUE(ble_a.IsAvailable());
  EXPECT_TRUE(ble_b.IsMediumValid());
  EXPECT_TRUE(ble_b.IsAvailable());
  EXPECT_NE(&radio_a.GetBluetoothAdapter(), &radio_b.GetBluetoothAdapter());
  env_.Stop();
}

TEST_F(BleV2Test, CanStartFastAdvertising) {
  env_.Start();
  BluetoothRadio radio;
  BleV2 ble{radio};
  radio.Enable();
  ByteArray advertisement_bytes{std::string(kAdvertisementString)};

  EXPECT_TRUE(ble.StartAdvertising(std::string(kServiceIDA),
                                   advertisement_bytes, PowerLevel::kHighPower,
                                   std::string(kFastAdvertisementServiceUuid)));
  // Can't advertise twice for the same service_id.
  EXPECT_FALSE(ble.StartAdvertising(
      std::string(kServiceIDA), advertisement_bytes, PowerLevel::kHighPower,
      std::string(kFastAdvertisementServiceUuid)));
  EXPECT_TRUE(ble.StopAdvertising(std::string(kServiceIDA)));
  env_.Stop();
}

TEST_F(BleV2Test, CanStartAdvertising) {
  env_.Start();
  BluetoothRadio radio;
  BleV2 ble{radio};
  radio.Enable();
  ByteArray advertisement_bytes{std::string(kAdvertisementString)};

  EXPECT_TRUE(ble.StartAdvertising(std::string(kServiceIDA),
                                   advertisement_bytes, PowerLevel::kHighPower,
                                   ""));
  // Can't advertise twice for the same service_id.
  EXPECT_FALSE(ble.StartAdvertising(std::string(kServiceIDA),
                                    advertisement_bytes, PowerLevel::kHighPower,
                                    ""));
  EXPECT_TRUE(ble.StopAdvertising(std::string(kServiceIDA)));
  env_.Stop();
}

TEST_F(BleV2Test, CanStartScanning) {
  env_.Start();
  BluetoothRadio radio;
  BleV2 ble{radio};
  radio.Enable();

  EXPECT_TRUE(ble.StartScanning(std::string(kServiceIDA),
                                PowerLevel::kHighPower,
                                mediums::DiscoveredPeripheralCallback{
                                    .peripheral_discovered_cb =
                                        [](mediums::BlePeripheral& peripheral,
                                           const std::string& service_id,
                                           const ByteArray& advertisement_bytes,
                                           bool fast_advertisement) {
                                          // nothing to do for now
                                        },
                                    .peripheral_lost_cb =
                                        [](mediums::BlePeripheral& peripheral,
                                           const std::string& service_id) {
                                          // nothing to do for now
                                        },
                                },
                                ""));
  EXPECT_TRUE(ble.StopScanning(std::string(kServiceIDA)));
  env_.Stop();
}

TEST_F(BleV2Test, CanStartFastAdvertising) {
  env_.Start();
  BluetoothRadio radio_a;
  BluetoothRadio radio_b;
  BleV2 ble_a{radio_a};
  BleV2 ble_b{radio_b};
  radio_a.Enable();
  radio_b.Enable();
  ByteArray advertisement_bytes{std::string(kAdvertisementString)};
  CountDownLatch found_latch(1);

  ble_b.StartScanning(
      std::string(kServiceIDA), PowerLevel::kHighPower,
      mediums::DiscoveredPeripheralCallback{
          .peripheral_discovered_cb =
              [&found_latch](BleV2Peripheral& peripheral,
                             const std::string& service_id,
                             const ByteArray& advertisement_byts,
                             bool fast_advertisement) {
                EXPECT_TRUE(fast_advertisement);
                found_latch.CountDown();
              },
          .peripheral_lost_cb =
              [](BleV2Peripheral& peripheral, const std::string& service_id) {
                // nothing to do for now
              },
      },
      std::string(kFastAdvertisementServiceUuid));

  EXPECT_TRUE(ble_a.StartAdvertising(
      std::string(kServiceIDA), advertisement_bytes, PowerLevel::kHighPower,
      std::string(kFastAdvertisementServiceUuid)));
  EXPECT_TRUE(found_latch.Await(kWaitDuration).result());
  EXPECT_TRUE(ble_a.StopAdvertising(std::string(kServiceIDA)));
  ble_b.StopScanning(std::string(kServiceIDA));
  env_.Stop();
}

TEST_F(BleV2Test, CanStartFastScanning) {
  env_.Start();
  BluetoothRadio radio_a;
  BluetoothRadio radio_b;
  BleV2 ble_a{radio_a};
  BleV2 ble_b{radio_b};
  radio_a.Enable();
  radio_b.Enable();
  ByteArray advertisement_bytes{std::string(kAdvertisementString)};
  CountDownLatch found_latch(1);

  ble_b.StartAdvertising(std::string(kServiceIDA), advertisement_bytes,
                         PowerLevel::kHighPower,
                         std::string(kFastAdvertisementServiceUuid));

  EXPECT_TRUE(ble_a.StartScanning(
      std::string(kServiceIDA), PowerLevel::kHighPower,
      mediums::DiscoveredPeripheralCallback{
          .peripheral_discovered_cb =
              [&found_latch](
                  BleV2Peripheral& peripheral, const std::string& service_id,
                  const ByteArray& advertisement_byts,
                  bool fast_advertisement) { found_latch.CountDown(); },
          .peripheral_lost_cb =
              [](BleV2Peripheral& peripheral, const std::string& service_id) {
                // nothing to do for now
              },
      },
      std::string(kFastAdvertisementServiceUuid)));

  EXPECT_TRUE(found_latch.Await(kWaitDuration).result());
  ble_b.StopAdvertising(std::string(kServiceIDA));
  EXPECT_TRUE(ble_a.StopScanning(std::string(kServiceIDA)));
  env_.Stop();
}

TEST_F(BleV2Test, CanStartStopMultipleScanningWithDifferentServiceIds) {
  env_.Start();
  BluetoothRadio radio;
  BleV2 ble{radio};
  radio.Enable();

  EXPECT_TRUE(ble.StartScanning(std::string(kServiceIDA),
                                PowerLevel::kHighPower,
                                mediums::DiscoveredPeripheralCallback{},
                                std::string(kFastAdvertisementServiceUuid)));
  EXPECT_TRUE(ble.StartScanning(std::string(kServiceIDB),
                                PowerLevel::kHighPower,
                                mediums::DiscoveredPeripheralCallback{},
                                std::string(kFastAdvertisementServiceUuid)));
  EXPECT_TRUE(ble.StopScanning(std::string(kServiceIDA)));

  EXPECT_TRUE(ble.StartScanning(std::string(kServiceIDA),
                                PowerLevel::kHighPower,
                                mediums::DiscoveredPeripheralCallback{},
                                std::string(kFastAdvertisementServiceUuid)));
  EXPECT_TRUE(ble.StopScanning(std::string(kServiceIDA)));
  EXPECT_TRUE(ble.StopScanning(std::string(kServiceIDB)));
  env_.Stop();
}

TEST_F(BleV2Test, DestructWorksForStartAdvertisingAndScanningWithoutStop) {
  env_.Start();
  BluetoothRadio radio_a;
  BluetoothRadio radio_b;
  BleV2 ble_a{radio_a};
  BleV2 ble_b{radio_b};

  radio_a.Enable();
  radio_b.Enable();
  ByteArray advertisement_bytes{std::string(kAdvertisementString)};

  // Device A starts advertising with service IDA and IDB.
  EXPECT_TRUE(ble_a.StartAdvertising(
      std::string(kServiceIDA), advertisement_bytes, PowerLevel::kHighPower,
      std::string(kFastAdvertisementServiceUuid)));
  EXPECT_TRUE(ble_a.StartAdvertising(
      std::string(kServiceIDB), advertisement_bytes, PowerLevel::kHighPower,
      std::string(kFastAdvertisementServiceUuid)));

  // Device B starts scanning with service IDA and IDB
  EXPECT_TRUE(ble_b.StartScanning(std::string(kServiceIDA),
                                  PowerLevel::kHighPower,
                                  mediums::DiscoveredPeripheralCallback{},
                                  std::string(kFastAdvertisementServiceUuid)));
  EXPECT_TRUE(ble_b.StartScanning(std::string(kServiceIDB),
                                  PowerLevel::kHighPower,
                                  mediums::DiscoveredPeripheralCallback{},
                                  std::string(kFastAdvertisementServiceUuid)));
  env_.Stop();
}

}  // namespace
}  // namespace connections
}  // namespace nearby
}  // namespace location
