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

#include <string>
#include <utility>
#include <memory>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "fastpair/repository/device_metadata.h"
#include "fastpair/ui/actions.h"
#include "fastpair/ui/fast_pair/fake_fast_pair_notification_controller_observer.h"

namespace nearby {
namespace fastpair {
namespace {

const int64_t kDeviceId = 10148625;
const char kModelId[] = "9adb11";
const char kDeviceName[] = "Pixel Buds Pro";

class FastPairNotificationControllerTest : public ::testing::Test {
 protected:
  FastPairNotificationControllerTest() {
    notification_controller_.AddObserver(&notification_controller_obsesrver_);
  }

  void TriggerOnUpdateDevice(DeviceMetadata& device,
                             DiscoveryCallback callback) {
    notification_controller_.ShowGuestDiscoveryNotification(
        device, std::move(callback));
  }

  void DiscoveryActionClicked(DiscoveryAction action) {
    discovery_action_ = action;
  }

  FastPairNotificationController notification_controller_;
  FakeFastPairNotificationControllerObserver notification_controller_obsesrver_;
  DiscoveryAction discovery_action_;
};

TEST_F(FastPairNotificationControllerTest, ShowGuestDiscoveryNotification) {
  proto::GetObservedDeviceResponse response;
  response.mutable_device()->set_id(kDeviceId);
  response.mutable_device()->set_name(kDeviceName);
  DeviceMetadata device_metadata(response);
  TriggerOnUpdateDevice(device_metadata, [this](DiscoveryAction action) {
    DiscoveryActionClicked(action);
  });
  EXPECT_TRUE(notification_controller_obsesrver_
                  .CheckDeviceMetadataListContainTestDevice(kDeviceName));
  EXPECT_EQ(1, notification_controller_obsesrver_.on_update_device_count());
  notification_controller_.OnDiscoveryClicked(DiscoveryAction::kPairToDevice);
  EXPECT_EQ(DiscoveryAction::kPairToDevice, discovery_action_);
}

}  // namespace
}  // namespace fastpair
}  // namespace nearby
