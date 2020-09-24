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

#include "core_v2/internal/mediums/ble.h"

#include <string>

#include "core_v2/internal/mediums/bluetooth_radio.h"
#include "platform_v2/base/medium_environment.h"
#include "platform_v2/public/ble.h"
#include "platform_v2/public/count_down_latch.h"
#include "platform_v2/public/logging.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace location {
namespace nearby {
namespace connections {
namespace {

constexpr absl::Duration kWaitDuration = absl::Milliseconds(1000);
constexpr absl::string_view kServiceID{"com.google.location.nearby.apps.test"};
constexpr absl::string_view kAdvertisementString{"\x0a\x0b\x0c\x0d"};
constexpr absl::string_view kFastAdvertisementServiceUuid{"\xf3\xfe"};

class BleTest : public ::testing::Test {
 protected:
  using DiscoveredPeripheralCallback = BleMedium::DiscoveredPeripheralCallback;

  BleTest() { env_.Stop(); }

  MediumEnvironment& env_{MediumEnvironment::Instance()};
};

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
      service_id,
      fast_advertisement_service_uuid,
      DiscoveredPeripheralCallback{
          .peripheral_discovered_cb =
              [&found_latch](
                  BlePeripheral& peripheral, const std::string& service_id,
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
      service_id,
      fast_advertisement_service_uuid,
      DiscoveredPeripheralCallback{
          .peripheral_discovered_cb =
              [&accept_latch](
                  BlePeripheral& peripheral, const std::string& service_id,
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

TEST_F(BleTest, CanStartAcceptingConnectionsAndConnect) {
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
      {
          .accepted_cb = [&accept_latch](
                             BleSocket socket,
                             const std::string&) { accept_latch.CountDown(); },
      });
  BlePeripheral discovered_peripheral;
  ble_b.StartScanning(
      service_id,
      fast_advertisement_service_uuid,
      {
          .peripheral_discovered_cb =
              [&found_latch, &discovered_peripheral](
                  BlePeripheral& peripheral, const std::string& service_id,
                  bool fast_advertisement) {
                discovered_peripheral = peripheral;
                NEARBY_LOG(
                    INFO,
                    "Discovered peripheral=%p [impl=%p], fast advertisement=%d",
                    &peripheral, &peripheral.GetImpl(), fast_advertisement);
                found_latch.CountDown();
              },
      });

  EXPECT_TRUE(found_latch.Await(kWaitDuration).result());
  ASSERT_TRUE(discovered_peripheral.IsValid());

  BleSocket socket =
      ble_b.Connect(discovered_peripheral, service_id);

  EXPECT_TRUE(accept_latch.Await(kWaitDuration).result());
  EXPECT_TRUE(socket.IsValid());
  ble_b.StopScanning(service_id);
  ble_a.StopAdvertising(service_id);
  env_.Stop();
}

}  // namespace
}  // namespace connections
}  // namespace nearby
}  // namespace location
