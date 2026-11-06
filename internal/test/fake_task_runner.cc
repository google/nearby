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

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "internal/platform/implementation/executor.h"
#include "internal/platform/multi_thread_executor.h"
#include "internal/platform/single_thread_executor.h"
#include "internal/platform/submittable_executor.h"
#include "internal/platform/timer.h"
#include "internal/test/fake_clock.h"
#include "internal/test/fake_timer.h"

namespace nearby {

FakeTaskRunner::FakeTaskRunner(FakeClock* clock, uint32_t count)
    : FakeTaskRunner(clock,
                     count == 1
                         ? std::unique_ptr<SubmittableExecutor>(
                               std::make_unique<SingleThreadExecutor>())
                         : std::unique_ptr<SubmittableExecutor>(
                               std::make_unique<MultiThreadExecutor>(count))) {}

FakeTaskRunner::FakeTaskRunner(
    FakeClock* clock, std::unique_ptr<SubmittableExecutor> task_executor)
    : clock_(clock), task_executor_(std::move(task_executor)) {}

FakeTaskRunner::~FakeTaskRunner() { Shutdown(); }

void FakeTaskRunner::Shutdown() {
  std::vector<std::unique_ptr<Timer>> timers;
  {
    absl::MutexLock lock(mutex_);
    if (closed_) {
      return;
    }
    closed_ = true;
    timers = std::move(timers_);
  }
  for (auto& timer : timers) {
    timer->Stop();
  }
  task_executor_->Shutdown();
}

bool FakeTaskRunner::PostTask(absl::AnyInvocable<void()> task) {
  {
    absl::MutexLock lock(mutex_);
    if (closed_) {
      return false;
    }
  }
  if (task) {
    task_executor_->Execute([task = std::move(task)]() mutable { task(); });
  }
  return true;
}

bool FakeTaskRunner::PostDelayedTask(absl::Duration delay,
                                     absl::AnyInvocable<void()> task) {
  absl::MutexLock lock(mutex_);
  if (closed_) {
    return false;
  }
  if (!task) {
    return true;
  }
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
  if (PostTask([&] { notification.Notify(); })) {
    notification.WaitForNotification();
  }
}

bool FakeTaskRunner::SyncWithTimeout(absl::Duration timeout) {
  absl::Notification notification;
  if (!PostTask([&] { notification.Notify(); })) {
    return false;
  }
  return notification.WaitForNotificationWithTimeout(timeout);
}

FakeSingleThreadTaskRunner::FakeSingleThreadTaskRunner(FakeClock* clock)
    : FakeSingleThreadTaskRunner(clock,
                                 std::make_unique<SingleThreadExecutor>()) {}

FakeSingleThreadTaskRunner::FakeSingleThreadTaskRunner(
    FakeClock* clock, std::unique_ptr<SingleThreadExecutor> executor)
    : thread_id_(executor->GetThreadId()), impl_(clock, std::move(executor)) {}

FakeSingleThreadTaskRunner::~FakeSingleThreadTaskRunner() = default;

bool FakeSingleThreadTaskRunner::PostTask(absl::AnyInvocable<void()> task) {
  return impl_.PostTask(std::move(task));
}

bool FakeSingleThreadTaskRunner::PostDelayedTask(
    absl::Duration delay, absl::AnyInvocable<void()> task) {
  return impl_.PostDelayedTask(delay, std::move(task));
}

void FakeSingleThreadTaskRunner::Shutdown() { impl_.Shutdown(); }

ThreadId FakeSingleThreadTaskRunner::GetThreadId() const { return thread_id_; }

void FakeSingleThreadTaskRunner::Sync() { impl_.Sync(); }

bool FakeSingleThreadTaskRunner::SyncWithTimeout(absl::Duration timeout) {
  return impl_.SyncWithTimeout(timeout);
}

}  // namespace nearby
