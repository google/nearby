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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_INTERNAL_IMPL_G3_TIMER_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_INTERNAL_IMPL_G3_TIMER_H_

#include <functional>
#include <utility>

#include "fastpair/internal/api/timer.h"

namespace location {
namespace nearby {
namespace g3 {

class Timer : public api::Timer {
 public:
  Timer() = default;
  ~Timer() override = default;

  bool Create(int delay, int interval,
              std::function<void()> callback) override {
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
  std::function<void()> callback_;
  bool is_stopped_ = false;
};

}  // namespace g3
}  // namespace nearby
}  // namespace location

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_INTERNAL_IMPL_G3_TIMER_H_
