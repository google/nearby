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

#ifndef PLATFORM_PUBLIC_TIMER_IMPL_H_
#define PLATFORM_PUBLIC_TIMER_IMPL_H_

#include <memory>

#include "absl/functional/any_invocable.h"
#include "internal/platform/implementation/platform.h"
#include "internal/platform/timer.h"

namespace nearby {
class TimerImpl : public Timer {
 public:
  ~TimerImpl() override { Stop(); }

  bool Start(int delay, int period,
             absl::AnyInvocable<void()> callback) override;
  bool Stop() override;
  bool IsRunning() override;
  bool FireNow() override;

 private:
  int delay_ = 0;
  int period_ = 0;
  std::unique_ptr<api::Timer> internal_timer_ = nullptr;
};

}  // namespace nearby

#endif  // PLATFORM_PUBLIC_TIMER_IMPL_H_
