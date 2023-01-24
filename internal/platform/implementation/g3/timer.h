// Copyright 2021 Google LLC
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

#ifndef PLATFORM_IMPL_G3_TIMER_H_
#define PLATFORM_IMPL_G3_TIMER_H_

#include <utility>

#include "internal/platform/implementation/timer.h"

namespace nearby {
namespace g3 {

class Timer : public api::Timer {
 public:
  Timer() = default;
  ~Timer() override = default;

  bool Create(int delay, int interval,
              absl::AnyInvocable<void()> callback) override {
    if (delay < 0 || interval < 0) {
      return false;
    }
    callback_ = std::move(callback);
    is_stopped_ = false;
    return true;
  }

  bool Stop() override {
    is_stopped_ = true;
    return true;
  }

  bool FireNow() override {
    if (is_stopped_ || !callback_) {
      return false;
    }
    callback_();
    return true;
  }

  // Mocked methods for test only
  void TriggerCallback() {
    if (is_stopped_ || callback_ == nullptr) {
      return;
    }

    callback_();
  }

 private:
  absl::AnyInvocable<void()> callback_;
  bool is_stopped_ = false;
};

}  // namespace g3
}  // namespace nearby

#endif  // PLATFORM_IMPL_G3_TIMER_H_
