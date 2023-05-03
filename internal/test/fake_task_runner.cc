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
#include <future>  // NOLINT
#include <memory>
#include <utility>
#include <vector>

#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "internal/test/fake_timer.h"

namespace nearby {

std::atomic_uint FakeTaskRunner::total_running_thread_count_ = 0;

FakeTaskRunner::~FakeTaskRunner() {
  absl::MutexLock lock(&mutex_);
  CleanThreads();
}

bool FakeTaskRunner::PostTask(absl::AnyInvocable<void()> task) {
  absl::MutexLock lock(&mutex_);
  if (mode_ == Mode::kActive) {
    if (running_thread_count_ >= count_) {
      queued_tasks_.push_back(std::move(task));
      return true;
    }

    ++running_thread_count_;
    Run(std::move(task));
    return true;
  }
  pending_tasks_.push_back(std::move(task));
  return true;
}

bool FakeTaskRunner::PostDelayedTask(absl::Duration delay,
                                     absl::AnyInvocable<void()> task) {
  absl::MutexLock lock(&mutex_);
  std::unique_ptr<Timer> timer = std::make_unique<FakeTimer>(clock_);
  Timer* timer_ptr = timer.get();
  uint32_t id = GenerateId();
  queued_delayed_tasks_.emplace(id, std::move(timer));
  timer_ptr->Start(delay / absl::Milliseconds(1), 0,
                   [this, task = std::move(task), id]() mutable {
                     PostTask(std::move(task));
                     {
                       absl::MutexLock lock(&mutex_);
                       completed_delayed_tasks_.push_back(id);
                     }
                   });
  return true;
}

void FakeTaskRunner::SetMode(Mode mode) {
  absl::MutexLock lock(&mutex_);
  mode_ = mode;
}

FakeTaskRunner::Mode FakeTaskRunner::GetMode() const {
  absl::MutexLock lock(&mutex_);
  return mode_;
}

void FakeTaskRunner::RunNextPendingTask() {
  absl::MutexLock lock(&mutex_);
  InternalRunNextPendingTask();
}

void FakeTaskRunner::RunAllPendingTasks() {
  absl::MutexLock lock(&mutex_);
  while (!pending_tasks_.empty()) {
    InternalRunNextPendingTask();
  }
}

void FakeTaskRunner::Sync() {
  absl::Notification notification;
  PostTask([&] { notification.Notify(); });
  notification.WaitForNotification();
}

const std::vector<absl::AnyInvocable<void()>>&
FakeTaskRunner::GetAllPendingTasks() const {
  absl::MutexLock lock(&mutex_);
  return pending_tasks_;
}

const absl::flat_hash_map<uint32_t, std::unique_ptr<Timer>>&
FakeTaskRunner::GetAllDelayedTasks() {
  absl::MutexLock lock(&mutex_);
  if (!completed_delayed_tasks_.empty()) {
    for (uint32_t id : completed_delayed_tasks_) {
      queued_delayed_tasks_.erase(id);
    }
  }
  return queued_delayed_tasks_;
}

int FakeTaskRunner::GetConcurrentCount() const {
  absl::MutexLock lock(&mutex_);
  return count_;
}

bool FakeTaskRunner::WaitForRunningTasksWithTimeout(absl::Duration timeout) {
  int i = (timeout / absl::Milliseconds(1)) / 50;
  while (total_running_thread_count_ != 0 && i > 0) {
    absl::SleepFor(absl::Milliseconds(50));
    --i;
  }

  return total_running_thread_count_ == 0;
}

uint32_t FakeTaskRunner::GenerateId() {
  ++current_id_;
  return current_id_;
}

void FakeTaskRunner::CleanThreads() {
  auto it = threads_.begin();
  while (it != threads_.end()) {
    // Delete the thread if it is ready
    auto status = it->wait_for(std::chrono::seconds(0));
    if (status == std::future_status::ready) {
      it = threads_.erase(it);
    } else {
      ++it;
    }
  }
}

void FakeTaskRunner::Run(absl::AnyInvocable<void()> task) {
  CleanThreads();
  ++total_running_thread_count_;
  // Run the task in a new thread, to simulate the real environment.
  std::future<void> thread =
      std::async(std::launch::async, [&, task = std::move(task)]() mutable {
        task();
        RunNextQueueTask();
        --total_running_thread_count_;
      });
  threads_.push_back(std::move(thread));
}

void FakeTaskRunner::InternalRunNextPendingTask() {
  if (pending_tasks_.empty()) {
    return;
  }

  Run(std::move(pending_tasks_.front()));
  pending_tasks_.erase(pending_tasks_.begin());
}

void FakeTaskRunner::RunNextQueueTask() {
  absl::MutexLock lock(&mutex_);
  --running_thread_count_;
  if (queued_tasks_.empty()) {
    return;
  }

  auto task = std::move(queued_tasks_.front());
  queued_tasks_.erase(queued_tasks_.begin());
  ++running_thread_count_;
  Run(std::move(task));
}

}  // namespace nearby
