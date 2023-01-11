// Copyright 2020 Google LLC
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

#include "internal/platform/implementation/g3/scheduled_executor.h"

#include <atomic>
#include <memory>
#include <utility>

#include "absl/time/clock.h"
#include "internal/platform/implementation/cancelable.h"
#include "internal/platform/runnable.h"

namespace nearby {
namespace g3 {

namespace {

class ScheduledCancelable : public api::Cancelable {
 public:
  bool Cancel() override {
    Status expected = kNotRun;
    while (expected == kNotRun) {
      if (status_.compare_exchange_strong(expected, kCanceled)) {
        return true;
      }
    }
    return false;
  }
  bool MarkExecuted() {
    Status expected = kNotRun;
    while (expected == kNotRun) {
      if (status_.compare_exchange_strong(expected, kExecuted)) {
        return true;
      }
    }
    return false;
  }

 private:
  enum Status {
    kNotRun,
    kExecuted,
    kCanceled,
  };
  std::atomic<Status> status_ = kNotRun;
};

}  // namespace

std::shared_ptr<api::Cancelable> ScheduledExecutor::Schedule(
    Runnable&& runnable, absl::Duration delay) {
  auto scheduled_cancelable = std::make_shared<ScheduledCancelable>();
  if (executor_.InShutdown()) {
    return scheduled_cancelable;
  }
  executor_.ScheduleAfter(delay, [this, scheduled_cancelable,
                                  runnable(std::move(runnable))]() mutable {
    if (!executor_.InShutdown() && scheduled_cancelable->MarkExecuted()) {
      runnable();
    }
  });
  return scheduled_cancelable;
}

}  // namespace g3
}  // namespace nearby
