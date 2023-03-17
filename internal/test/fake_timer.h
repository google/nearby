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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_TEST_FAKE_TIMER_H_
#define THIRD_PARTY_NEARBY_INTERNAL_TEST_FAKE_TIMER_H_

#include <string>

#include "internal/platform/timer.h"
#include "internal/test/fake_clock.h"

namespace nearby {
class FakeTimer : public Timer {
 public:
  explicit FakeTimer(FakeClock* clock);
  ~FakeTimer() override;
  FakeTimer(FakeTimer&&) = default;
  FakeTimer& operator=(FakeTimer&&) = default;

  bool Start(int delay, int period,
             absl::AnyInvocable<void()> callback) override;
  bool Stop() override;
  bool IsRunning() override;
  bool FireNow() override;

 private:
  void ClockUpdated();

  std::string id_;
  int delay_ = 0;
  int period_ = false;
  int fired_count_ = 0;
  absl::Time start_time_;
  bool is_started_ = false;
  absl::AnyInvocable<void()> callback_;
  FakeClock* clock_ = nullptr;
};

}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_TEST_FAKE_TIMER_H_
