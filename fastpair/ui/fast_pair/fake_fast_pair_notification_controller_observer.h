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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_UI_FAST_PAIR_FAKE_FAST_PAIR_NOTIFICATION_CONTROLLER_OBSERVER_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_UI_FAST_PAIR_FAKE_FAST_PAIR_NOTIFICATION_CONTROLLER_OBSERVER_H_

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "fastpair/repository/device_metadata.h"
#include "fastpair/ui/fast_pair/fast_pair_notification_controller.h"
#include "internal/platform/count_down_latch.h"

namespace nearby {
namespace fastpair {
class FakeFastPairNotificationControllerObserver
    : public FastPairNotificationController::Observer {
 public:
  explicit FakeFastPairNotificationControllerObserver(
      std::optional<CountDownLatch> latch) {
    latch_ = latch;
  }

  void OnUpdateDevice(const DeviceMetadata& device) override {
    device_metadata_name_list_.push_back(device.GetDetails().name());
    on_update_device_count_++;
    if (latch_.has_value()) {
      latch_->CountDown();
    }
  }

  bool CheckDeviceMetadataListContainTestDevice(
      const std::string& device_name) {
    auto it = std::find(device_metadata_name_list_.begin(),
                        device_metadata_name_list_.end(), device_name);
    return it != device_metadata_name_list_.end();
  }

  int on_update_device_count() { return on_update_device_count_; }

  std::vector<std::string> device_metadata_name_list() {
    return device_metadata_name_list_;
  }

 private:
  std::vector<std::string> device_metadata_name_list_;
  int on_update_device_count_ = 0;
  std::optional<CountDownLatch> latch_;
};
}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_UI_FAST_PAIR_FAKE_FAST_PAIR_NOTIFICATION_CONTROLLER_OBSERVER_H_
