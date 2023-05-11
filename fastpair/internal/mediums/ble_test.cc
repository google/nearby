// Copyright 2023 Google LLC
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

#include "fastpair/internal/mediums/ble.h"

#include <string>

#include "gtest/gtest.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/medium_environment.h"

namespace nearby {
namespace fastpair {
namespace {

using DiscoveredPeripheralCallback = BleMedium::DiscoveredPeripheralCallback;
constexpr absl::Duration kWaitDuration = absl::Milliseconds(1000);
constexpr absl::string_view kServiceID{"com.google.location.nearby.apps.test"};
constexpr absl::string_view kAdvertisementString{"96C12E"};
constexpr absl::string_view kFastPairServiceUuid{"\x2c\xfe"};

TEST(BleTest, ConstructorDestructorWorks) {
  BluetoothRadio radio;
  Ble ble(radio);
  EXPECT_TRUE(ble.IsAvailable());
  EXPECT_FALSE(ble.IsScanning());
}

TEST(BleTest, CanStartDiscovery) {
  MediumEnvironment::Instance().Start();
  BluetoothRadio radio_a;
  BluetoothRadio radio_b;
  Ble ble_a{radio_a};
  Ble ble_b{radio_b};
  radio_a.Enable();
  radio_b.Enable();
  std::string service_id(kServiceID);
  ByteArray advertisement_bytes{std::string(kAdvertisementString)};
  std::string fast_pair_service_uuid(kFastPairServiceUuid);
  CountDownLatch accept_latch(1);
  CountDownLatch lost_latch(1);
  ble_b.GetMedium().StartAdvertising(service_id, advertisement_bytes,
                                     fast_pair_service_uuid);

  EXPECT_TRUE(ble_a.StartScanning(
      service_id, fast_pair_service_uuid,
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
  EXPECT_TRUE(ble_a.IsScanning());
  EXPECT_TRUE(accept_latch.Await(kWaitDuration).result());
  ble_b.GetMedium().StopAdvertising(service_id);
  EXPECT_TRUE(lost_latch.Await(kWaitDuration).result());
  EXPECT_TRUE(ble_a.StopScanning(service_id));
  EXPECT_FALSE(ble_a.IsScanning());
  MediumEnvironment::Instance().Stop();
}
}  // namespace
}  // namespace fastpair
}  // namespace nearby
