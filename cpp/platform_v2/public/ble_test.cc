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

#include "platform_v2/public/ble.h"

#include <memory>

#include "platform_v2/base/medium_environment.h"
#include "platform_v2/public/count_down_latch.h"
#include "platform_v2/public/logging.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace location {
namespace nearby {
namespace {

constexpr absl::Duration kWaitDuration = absl::Milliseconds(1000);
constexpr absl::string_view kServiceID{"com.google.location.nearby.apps.test"};
constexpr absl::string_view kAdvertisementString{"\x0a\x0b\x0c\x0d"};
constexpr absl::string_view kFastAdvertisementServiceUuid{"\xf3\xfe"};

class BleMediumTest : public ::testing::Test {
 protected:
  using DiscoveredPeripheralCallback = BleMedium::DiscoveredPeripheralCallback;
  using AcceptedConnectionCallback = BleMedium::AcceptedConnectionCallback;

  BleMediumTest() { env_.Stop(); }

  MediumEnvironment& env_{MediumEnvironment::Instance()};
};

TEST_F(BleMediumTest, ConstructorDestructorWorks) {
  env_.Start();
  BluetoothAdapter adapter_a_;
  BluetoothAdapter adapter_b_;
  BleMedium ble_a{adapter_a_};
  BleMedium ble_b{adapter_b_};

  // Make sure we can create functional mediums.
  ASSERT_TRUE(ble_a.IsValid());
  ASSERT_TRUE(ble_b.IsValid());

  // Make sure we can create 2 distinct mediums.
  EXPECT_NE(&ble_a.GetImpl(), &ble_b.GetImpl());
  env_.Stop();
}

TEST_F(BleMediumTest, CanStartAdvertising) {
  env_.Start();
  BluetoothAdapter adapter_a_;
  BluetoothAdapter adapter_b_;
  BleMedium ble_a{adapter_a_};
  BleMedium ble_b{adapter_b_};
  std::string service_id(kServiceID);
  ByteArray advertisement_bytes{std::string(kAdvertisementString)};
  std::string fast_advertisement_service_uuid(kFastAdvertisementServiceUuid);
  CountDownLatch found_latch(1);

  ble_a.StartAdvertising(service_id, advertisement_bytes,
                         fast_advertisement_service_uuid);

  EXPECT_TRUE(ble_b.StartScanning(
      service_id,
      fast_advertisement_service_uuid,
      DiscoveredPeripheralCallback{
          .peripheral_discovered_cb =
              [&found_latch](
                  BlePeripheral& peripheral, const std::string& service_id,
                  bool fast_advertisement) { found_latch.CountDown(); },
      }));
  EXPECT_TRUE(found_latch.Await(kWaitDuration).result());
  EXPECT_TRUE(ble_a.StopAdvertising(service_id));
  EXPECT_TRUE(ble_b.StopScanning(service_id));
  env_.Stop();
}

TEST_F(BleMediumTest, CanStartScanning) {
  env_.Start();
  BluetoothAdapter adapter_a_;
  BluetoothAdapter adapter_b_;
  BleMedium ble_a{adapter_a_};
  BleMedium ble_b{adapter_b_};
  std::string service_id(kServiceID);
  ByteArray advertisement_bytes{std::string(kAdvertisementString)};
  std::string fast_advertisement_service_uuid(kFastAdvertisementServiceUuid);
  CountDownLatch found_latch(1);
  CountDownLatch lost_latch(1);

  ble_a.StartScanning(
      service_id,
      fast_advertisement_service_uuid,
      DiscoveredPeripheralCallback{
          .peripheral_discovered_cb =
              [&found_latch](
                  BlePeripheral& peripheral, const std::string& service_id,
                  bool fast_advertisement) { found_latch.CountDown(); },
          .peripheral_lost_cb =
              [&lost_latch](BlePeripheral& peripheral,
                            const std::string& service_id) {
                lost_latch.CountDown();
              },
      });
  EXPECT_TRUE(ble_b.StartAdvertising(service_id, advertisement_bytes,
                                     fast_advertisement_service_uuid));
  EXPECT_TRUE(found_latch.Await(kWaitDuration).result());
  EXPECT_TRUE(ble_b.StopAdvertising(service_id));
  EXPECT_TRUE(lost_latch.Await(kWaitDuration).result());
  EXPECT_TRUE(ble_a.StopScanning(service_id));
  env_.Stop();
}

TEST_F(BleMediumTest, CanStopDiscovery) {
  env_.Start();
  BluetoothAdapter adapter_a_;
  BluetoothAdapter adapter_b_;
  BleMedium ble_a{adapter_a_};
  BleMedium ble_b{adapter_b_};
  std::string service_id(kServiceID);
  ByteArray advertisement_bytes{std::string(kAdvertisementString)};
  std::string fast_advertisement_service_uuid(kFastAdvertisementServiceUuid);
  CountDownLatch found_latch(1);
  CountDownLatch lost_latch(1);

  ble_a.StartScanning(
      service_id,
      fast_advertisement_service_uuid,
      DiscoveredPeripheralCallback{
          .peripheral_discovered_cb =
              [&found_latch](
                  BlePeripheral& peripheral, const std::string& service_id,
                  bool fast_advertisement) { found_latch.CountDown(); },
          .peripheral_lost_cb =
              [&lost_latch](BlePeripheral& peripheral,
                            const std::string& service_id) {
                lost_latch.CountDown();
              },
      });
  EXPECT_TRUE(ble_b.StartAdvertising(service_id, advertisement_bytes,
                                     fast_advertisement_service_uuid));
  EXPECT_TRUE(found_latch.Await(kWaitDuration).result());
  EXPECT_TRUE(ble_a.StopScanning(service_id));
  EXPECT_TRUE(ble_b.StopAdvertising(service_id));
  EXPECT_FALSE(lost_latch.Await(kWaitDuration).result());
  env_.Stop();
}

TEST_F(BleMediumTest, CanStartAcceptingConnectionsAndConnect) {
  env_.Start();
  BluetoothAdapter adapter_a_;
  BluetoothAdapter adapter_b_;
  BleMedium ble_a{adapter_a_};
  BleMedium ble_b{adapter_b_};
  std::string service_id(kServiceID);
  ByteArray advertisement_bytes{std::string(kAdvertisementString)};
  std::string fast_advertisement_service_uuid(kFastAdvertisementServiceUuid);
  CountDownLatch found_latch(1);
  CountDownLatch accepted_latch(1);

  BlePeripheral* discovered_peripheral = nullptr;
  ble_a.StartScanning(
      service_id,
      fast_advertisement_service_uuid,
      DiscoveredPeripheralCallback{
          .peripheral_discovered_cb =
              [&found_latch, &discovered_peripheral](
                  BlePeripheral& peripheral, const std::string& service_id,
                  bool fast_advertisement) {
                NEARBY_LOG(
                    INFO,
                    "Peripheral discovered: %s, %p, fast advertisement: %d",
                    peripheral.GetName().c_str(), &peripheral,
                    fast_advertisement);
                discovered_peripheral = &peripheral;
                found_latch.CountDown();
              },
      });
  ble_b.StartAdvertising(service_id, advertisement_bytes,
                         fast_advertisement_service_uuid);
  ble_b.StartAcceptingConnections(
      service_id,
      AcceptedConnectionCallback{
          .accepted_cb = [&accepted_latch](BleSocket socket,
                                           const std::string& service_id) {
            NEARBY_LOG(INFO, "Connection accepted: socket=%p, service_id=%s",
                       &socket, service_id.c_str());
            accepted_latch.CountDown();
          }});
  EXPECT_TRUE(found_latch.Await(kWaitDuration).result());

  BleSocket socket_a;
  EXPECT_FALSE(socket_a.IsValid());
  {
    SingleThreadExecutor client_executor;
    client_executor.Execute(
        [&ble_a, &socket_a, discovered_peripheral, &service_id]() {
          socket_a = ble_a.Connect(*discovered_peripheral, service_id);
        });
  }
  EXPECT_TRUE(accepted_latch.Await(kWaitDuration).result());
  EXPECT_TRUE(socket_a.IsValid());
  ble_b.StopAdvertising(service_id);
  ble_a.StopScanning(service_id);
  env_.Stop();
}

}  // namespace
}  // namespace nearby
}  // namespace location
