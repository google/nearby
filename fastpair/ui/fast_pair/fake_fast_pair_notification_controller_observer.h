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

#include "fastpair/common/device_metadata.h"
#include "fastpair/common/fast_pair_device.h"
#include "fastpair/ui/fast_pair/fast_pair_notification_controller.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/mutex_lock.h"

namespace nearby {
namespace fastpair {
class FakeFastPairNotificationControllerObserver
    : public FastPairNotificationController::Observer {
 public:
  explicit FakeFastPairNotificationControllerObserver(
      CountDownLatch* on_device_updated_latch,
      CountDownLatch* on_pairing_result_latch) {
    on_device_updated_latch_ = on_device_updated_latch;
    on_pairing_result_latch_ = on_pairing_result_latch;
  }

  void OnUpdateDevice(FastPairDevice& device) override {
    MutexLock lock(&mutex_);
    device_ = &const_cast<FastPairDevice&>(device);
    if (on_device_updated_latch_) {
      on_device_updated_latch_->CountDown();
    }
  }

  void OnPairingResult(FastPairDevice& device, bool success) override {
    MutexLock lock(&mutex_);
    pairing_result_ = success;
    device_ = &const_cast<FastPairDevice&>(device);
    if (on_pairing_result_latch_) {
      on_pairing_result_latch_->CountDown();
    }
  }

  FastPairDevice* GetDevice() {
    MutexLock lock(&mutex_);
    return device_;
  }

  std::optional<bool> GetPairingResult() {
    MutexLock lock(&mutex_);
    return pairing_result_;
  }

 private:
  Mutex mutex_;
  CountDownLatch* on_device_updated_latch_;
  CountDownLatch* on_pairing_result_latch_;
  FastPairDevice* device_ ABSL_GUARDED_BY(mutex_) = nullptr;
  std::optional<bool> pairing_result_ ABSL_GUARDED_BY(mutex_);
};
}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_UI_FAST_PAIR_FAKE_FAST_PAIR_NOTIFICATION_CONTROLLER_OBSERVER_H_
