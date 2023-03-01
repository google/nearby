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

#include <memory>
#include <vector>
#include <algorithm>
#include <string>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "fastpair/repository/device_metadata.h"
#include "internal/platform/medium_environment.h"

namespace nearby {
namespace fastpair {
namespace {

const int64_t kDeviceId = 10148625;
const char kModelId[] = "9adb11";
const char kDeviceName[] = "Pixel Buds Pro";

class FastPairNotificationControllerObserver
    : public FastPairNotificationController::Observer {
 public:
  void OnUpdateDevice(DeviceMetadata& device) override {
    device_metadata_name_list_.push_back(device.GetDetails().name());
    on_update_device_count_++;
  }

  bool CheckDeviceMetadataListContainTestDevice(
      const std::string& device_name) {
    auto it = std::find(device_metadata_name_list_.begin(),
                        device_metadata_name_list_.end(), device_name);
    return it != device_metadata_name_list_.end();
  }

  int on_update_device_count() { return on_update_device_count_; }

 private:
  std::vector<std::string> device_metadata_name_list_;
  int on_update_device_count_ = 0;
};

class FastPairNotificationControllerTest : public ::testing::Test {
 protected:
  FastPairNotificationControllerTest() {}
  ~FastPairNotificationControllerTest() override = default;
  void SetUp() override {
    notification_controller_ =
        std::make_shared<FastPairNotificationController>();
    notification_controller_obsesrver_ =
        std::make_unique<FastPairNotificationControllerObserver>();
    notification_controller_->AddObserver(
        notification_controller_obsesrver_.get());
  }

  void TearDown() override { env_.Stop(); }

  void TriggerOnUpdateDevice(DeviceMetadata& device) {
    notification_controller_->ShowGuestDiscoveryNotification(device);
  }

  MediumEnvironment& env_{MediumEnvironment::Instance()};
  std::shared_ptr<FastPairNotificationController> notification_controller_;
  std::unique_ptr<FastPairNotificationControllerObserver>
      notification_controller_obsesrver_;
};

TEST_F(FastPairNotificationControllerTest, ShowGuestDiscoveryNotification) {
  env_.Start();
  proto::GetObservedDeviceResponse response;
  response.mutable_device()->set_id(kDeviceId);
  response.mutable_device()->set_name(kDeviceName);
  DeviceMetadata device_metadata(response);
  TriggerOnUpdateDevice(device_metadata);
  EXPECT_TRUE(notification_controller_obsesrver_
                  ->CheckDeviceMetadataListContainTestDevice(kDeviceName));
  EXPECT_EQ(1, notification_controller_obsesrver_->on_update_device_count());
  env_.Stop();
}

}  // namespace
}  // namespace fastpair
}  // namespace nearby
