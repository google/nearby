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

#include "fastpair/ui/fast_pair/fast_pair_notification_controller.h"

#include <optional>

#include "gtest/gtest.h"
#include "fastpair/common/device_metadata.h"
#include "fastpair/ui/actions.h"
#include "fastpair/ui/fast_pair/fake_fast_pair_notification_controller_observer.h"
#include "internal/platform/count_down_latch.h"

namespace nearby {
namespace fastpair {
namespace {

const int64_t kDeviceId = 10148625;
const char kDeviceName[] = "Pixel Buds Pro";
constexpr absl::string_view kModelId("9adb11");
constexpr absl::string_view kBleAddress("AA:BB:CC:DD:EE:00");

TEST(FastPairNotificationControllerTest, ShowGuestDiscoveryNotification) {
  FastPairNotificationController notification_controller;
  proto::GetObservedDeviceResponse response;
  DeviceMetadata device_metadata(response);

  FastPairDevice device(kModelId, kBleAddress,
                        Protocol::kFastPairInitialPairing);
  device.SetMetadata(device_metadata);
  CountDownLatch on_update_device_latch(1);
  CountDownLatch on_click_latch(1);
  FakeFastPairNotificationControllerObserver observer(&on_update_device_latch,
                                                      nullptr);
  notification_controller.AddObserver(&observer);
  EXPECT_EQ(observer.GetDevice(), nullptr);
  DiscoveryAction discovery_action = DiscoveryAction::kUnknown;
  notification_controller.ShowGuestDiscoveryNotification(
      device, [&](DiscoveryAction action) {
        on_click_latch.CountDown();
        discovery_action = action;
      });
  on_update_device_latch.Await();
  EXPECT_EQ(observer.GetDevice(), &device);
  notification_controller.OnDiscoveryClicked(DiscoveryAction::kPairToDevice);
  on_click_latch.Await();
  EXPECT_EQ(discovery_action, DiscoveryAction::kPairToDevice);
}

TEST(FastPairNotificationControllerTest, ShowPairingResultNotification) {
  FastPairNotificationController notification_controller;
  proto::GetObservedDeviceResponse response;
  DeviceMetadata device_metadata(response);
  FastPairDevice device(kModelId, kBleAddress,
                        Protocol::kFastPairInitialPairing);
  device.SetMetadata(device_metadata);

  CountDownLatch on_pairing_result_latch(1);
  FakeFastPairNotificationControllerObserver observer(nullptr,
                                                      &on_pairing_result_latch);
  notification_controller.AddObserver(&observer);
  EXPECT_FALSE(observer.GetPairingResult().has_value());
  EXPECT_EQ(observer.GetDevice(), nullptr);
  notification_controller.ShowPairingResultNotification(device, true);
  on_pairing_result_latch.Await();
  EXPECT_EQ(observer.GetDevice(), &device);
  EXPECT_TRUE(observer.GetPairingResult().has_value());
  EXPECT_TRUE(observer.GetPairingResult().value());
}

}  // namespace
}  // namespace fastpair
}  // namespace nearby
