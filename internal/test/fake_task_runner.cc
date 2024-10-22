// Copyright 2022-2023 Google LLC
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

#include "internal/test/fake_task_runner.h"

#include <atomic>
#include <memory>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "internal/platform/timer.h"
#include "internal/test/fake_timer.h"

namespace nearby {

std::atomic_uint FakeTaskRunner::pending_tasks_count_ = 0;

FakeTaskRunner::~FakeTaskRunner() { Shutdown(); }

void FakeTaskRunner::Shutdown() {
  absl::MutexLock lock(&mutex_);
  task_executor_->Shutdown();
}

bool FakeTaskRunner::PostTask(absl::AnyInvocable<void()> task) {
  absl::MutexLock lock(&mutex_);
  ++pending_tasks_count_;
  task_executor_->Execute([task = std::move(task)]() mutable {
    task();
    --pending_tasks_count_;
  });
  return true;
}

bool FakeTaskRunner::PostDelayedTask(absl::Duration delay,
                                     absl::AnyInvocable<void()> task) {
  absl::MutexLock lock(&mutex_);
  std::unique_ptr<Timer> timer = std::make_unique<FakeTimer>(clock_);
  Timer* timer_ptr = timer.get();
  timers_.push_back(std::move(timer));
  timer_ptr->Start(
      delay / absl::Milliseconds(1), 0,
      [this, task = std::move(task)]() mutable { PostTask(std::move(task)); });
  return true;
}

void FakeTaskRunner::Sync() {
  absl::Notification notification;
  PostTask([&] { notification.Notify(); });
  notification.WaitForNotification();
}

bool FakeTaskRunner::SyncWithTimeout(absl::Duration timeout) {
  absl::Notification notification;
  PostTask([&] { notification.Notify(); });
  return notification.WaitForNotificationWithTimeout(timeout);
}

bool FakeTaskRunner::WaitForRunningTasksWithTimeout(absl::Duration timeout) {
  int i = (timeout / absl::Milliseconds(1)) / 50;
  while (pending_tasks_count_ != 0 && i > 0) {
    absl::SleepFor(absl::Milliseconds(50));
    --i;
  }
  return pending_tasks_count_ == 0;
}

}  // namespace nearby
