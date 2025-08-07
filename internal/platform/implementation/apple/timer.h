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

#ifndef PLATFORM_IMPL_APPLE_TIMER_H_
#define PLATFORM_IMPL_APPLE_TIMER_H_

#import <Foundation/Foundation.h>

#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/timer.h"

namespace nearby {
namespace apple {

class Timer : public api::Timer {
 public:
  Timer() = default;
  ~Timer() override ABSL_LOCKS_EXCLUDED(mutex_);

  bool Create(int delay, int interval,
              absl::AnyInvocable<void()> callback) override
      ABSL_LOCKS_EXCLUDED(mutex_);

  bool Stop() override ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  absl::Mutex mutex_;
  absl::CondVar condvar_ ABSL_GUARDED_BY(mutex_);
  bool callback_running_ ABSL_GUARDED_BY(mutex_) = false;
  absl::AnyInvocable<void()> callback_ ABSL_GUARDED_BY(mutex_);
  dispatch_source_t timer_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace apple
}  // namespace nearby

#endif  // PLATFORM_IMPL_APPLE_TIMER_H_
