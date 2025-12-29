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

#ifndef PLATFORM_IMPL_LINUX_TIMER_H_
#define PLATFORM_IMPL_LINUX_TIMER_H_

#include <signal.h>
#include <time.h>
#include <memory>
#include <optional>

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/linux/submittable_executor.h"
#include "internal/platform/implementation/timer.h"

namespace nearby {
namespace linux {

class Timer : public api::Timer {
 public:
  Timer() : timerid_(nullptr){};
  ~Timer() override;

  bool Create(int delay, int interval,
              absl::AnyInvocable<void()> callback) override
      ABSL_LOCKS_EXCLUDED(mutex_);
  bool Stop() override ABSL_LOCKS_EXCLUDED(mutex_);
  bool FireNow() override ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  absl::Mutex mutex_;
  std::optional<timer_t> timerid_ ABSL_GUARDED_BY(mutex_);
  absl::AnyInvocable<void()> callback_;
  std::unique_ptr<SubmittableExecutor> task_executor_;
};

}  // namespace linux
}  // namespace nearby

#endif  // PLATFORM_IMPL_LINUX_TIMER_H_
