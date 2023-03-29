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

#include "internal/platform/implementation/cancelable.h"
#include "internal/platform/medium_environment.h"
#include "internal/platform/runnable.h"
#include "internal/test/fake_clock.h"

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

ScheduledExecutor::ScheduledExecutor() {
  absl::optional<FakeClock*> fake_clock =
      MediumEnvironment::Instance().GetSimulatedClock();
  if (fake_clock.has_value()) {
    name_ = absl::StrFormat("G3 scheduled executor %p", this);
    (*fake_clock)->AddObserver(name_, [this]() { RunReadyTasks(); });
  }
}

ScheduledExecutor::~ScheduledExecutor() {
  absl::optional<FakeClock*> fake_clock =
      MediumEnvironment::Instance().GetSimulatedClock();
  if (fake_clock.has_value()) {
    (*fake_clock)->RemoveObserver(name_);
  }
  executor_.Shutdown();
}

std::shared_ptr<api::Cancelable> ScheduledExecutor::Schedule(
    Runnable&& runnable, absl::Duration delay) {
  auto scheduled_cancelable = std::make_shared<ScheduledCancelable>();
  if (executor_.InShutdown()) {
    return scheduled_cancelable;
  }
  Runnable task = [this, scheduled_cancelable,
                   runnable = std::move(runnable)]() mutable {
    if (!executor_.InShutdown() && scheduled_cancelable->MarkExecuted()) {
      runnable();
    }
  };
  absl::optional<FakeClock*> fake_clock =
      MediumEnvironment::Instance().GetSimulatedClock();
  if (fake_clock.has_value()) {
    absl::Time trigger_time = (*fake_clock)->Now() + delay;
    absl::MutexLock lock(&mutex_);
    tasks_.insert(std::pair<absl::Time, std::unique_ptr<Runnable>>(
        trigger_time, std::make_unique<Runnable>(std::move(task))));
  } else {
    executor_.ScheduleAfter(delay, std::move(task));
  }
  return scheduled_cancelable;
}

void ScheduledExecutor::RunReadyTasks() {
  absl::optional<FakeClock*> fake_clock =
      MediumEnvironment::Instance().GetSimulatedClock();
  if (executor_.InShutdown()) {
    return;
  }
  if (!fake_clock.has_value()) {
    return;
  }
  absl::Time current_time = (*fake_clock)->Now();
  absl::MutexLock lock(&mutex_);
  for (auto it = tasks_.begin(); it != tasks_.end();) {
    if (it->first <= current_time) {
      executor_.Execute(
          [task = std::move(it->second)]() mutable { (*task)(); });
      it = tasks_.erase(it);
    } else {
      // Tasks are sorted. We can stop iterating.
      break;
    }
  }
}

}  // namespace g3
}  // namespace nearby
