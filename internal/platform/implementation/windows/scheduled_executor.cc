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

#include "internal/platform/implementation/windows/scheduled_executor.h"

#include <crtdbg.h>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "internal/flags/nearby_flags.h"
#include "internal/platform/flags/nearby_platform_feature_flags.h"
#include "internal/platform/implementation/cancelable.h"
#include "internal/platform/implementation/system_clock.h"
#include "internal/platform/logging.h"
#include "internal/platform/runnable.h"

namespace nearby {
namespace windows {

ScheduledExecutor::ScheduledExecutor()
    : executor_(std::make_unique<nearby::windows::Executor>()),
      shut_down_(false) {}

// Cancelable is kept both in the executor context, and in the caller context.
// We want Cancelable to live until both caller and executor are done with it.
// Exclusive ownership model does not work for this case;
// using std:shared_ptr<> instead of std::unique_ptr<>.
std::shared_ptr<api::Cancelable> ScheduledExecutor::Schedule(
    Runnable&& runnable, absl::Duration duration) {
  absl::MutexLock lock(&mutex_);

  if (shut_down_) {
    NEARBY_LOGS(WARNING) << __func__
                         << ": Attempt to schedule on a shut down executor.";

    return nullptr;
  }

  if (NearbyFlags::GetInstance().GetBoolFlag(
          platform::config_package_nearby::nearby_platform_feature::
              kEnableTaskScheduler)) {
    return task_scheduler_.Schedule(std::move(runnable), duration);
  } else {
    absl::Time run_time = SystemClock::ElapsedRealtime() + duration;
    std::shared_ptr<ScheduledTask> task =
        std::make_shared<ScheduledTask>(std::move(runnable), run_time);

    scheduled_tasks_[run_time] = task;
    ReSchedule();
    return task;
  }
}

void ScheduledExecutor::Execute(Runnable&& runnable) {
  if (NearbyFlags::GetInstance().GetBoolFlag(
          platform::config_package_nearby::nearby_platform_feature::
              kEnableTaskScheduler)) {
    absl::MutexLock lock(&mutex_);
    executor_->Execute([runnable = std::move(runnable)]() mutable {
      if (runnable) {
        runnable();
      }
    });
  } else {
    {
      absl::MutexLock lock(&mutex_);
      if (notification_ != nullptr) {
        notification_->Notify();
      }
    }

    executor_->Execute([this, runnable = std::move(runnable)]() mutable {
      if (runnable) {
        runnable();
      }
      {
        absl::MutexLock lock(&mutex_);
        ReSchedule();
      }
    });
  }
}

void ScheduledExecutor::Shutdown() {
  {
    absl::MutexLock lock(&mutex_);
    if (shut_down_) {
      NEARBY_LOGS(WARNING) << __func__
                           << ": Attempt to Shutdown on a shut down executor.";
      return;
    }

    shut_down_ = true;
    if (NearbyFlags::GetInstance().GetBoolFlag(
            platform::config_package_nearby::nearby_platform_feature::
                kEnableTaskScheduler)) {
      task_scheduler_.Shutdown();
    } else {
      if (notification_ != nullptr) {
        notification_->Notify();
      }
    }
  }

  executor_->Shutdown();
  {
    absl::MutexLock lock(&mutex_);
    scheduled_tasks_.clear();
  }
}

bool ScheduledExecutor::ScheduledTask::Cancel() {
  absl::MutexLock lock(&mutex_);
  if (is_executed_.Get() || is_cancelled_.Get()) {
    return false;
  }

  is_cancelled_.Set(true);
  return true;
};

void ScheduledExecutor::ScheduledTask::Run() {
  absl::MutexLock lock(&mutex_);
  if (is_executed_.Get() || is_cancelled_.Get()) {
    return;
  }

  is_executed_.Set(true);
  task_();
}

bool ScheduledExecutor::ScheduledTask::IsDone() const {
  return is_cancelled_.Get() || is_executed_.Get();
}

absl::Time ScheduledExecutor::ScheduledTask::run_time() const {
  absl::MutexLock lock(&mutex_);
  return run_time_;
}

void ScheduledExecutor::ReSchedule() {
  if (shut_down_) {
    return;
  }

  if (notification_ != nullptr) {
    notification_->Notify();
  }

  notification_ = std::make_unique<absl::Notification>();
  executor_->Execute([this]() {
    ClearCompletedTasks();
    std::optional<absl::Time> next_task_time = GetNextRunTime();
    if (!next_task_time.has_value()) {
      return;
    }

    absl::Notification* notification = nullptr;
    {
      absl::MutexLock lock(&mutex_);
      notification = notification_.get();
    }

    absl::Duration wait_time =
        (*next_task_time) - SystemClock::ElapsedRealtime();
    if (wait_time > absl::ZeroDuration() && notification != nullptr &&
        notification->WaitForNotificationWithTimeout(wait_time)) {
      return;
    }

    std::vector<std::shared_ptr<ScheduledTask>> tasks = FetchTasksToRun();
    for (auto& task : tasks) {
      if (task && !task->IsDone()) {
        task->Run();
        {
          absl::MutexLock lock(&mutex_);
          if (shut_down_) {
            return;
          }
        }
      }
    }

    {
      absl::MutexLock lock(&mutex_);
      ReSchedule();
    }
  });
}

void ScheduledExecutor::ClearCompletedTasks() {
  absl::MutexLock lock(&mutex_);

  auto it = scheduled_tasks_.begin();
  while (it != scheduled_tasks_.end()) {
    if (it->second->IsDone()) {
      it = scheduled_tasks_.erase(it);
    } else {
      ++it;
    }
  }
}

std::optional<absl::Time> ScheduledExecutor::GetNextRunTime() {
  absl::MutexLock lock(&mutex_);

  if (shut_down_ || scheduled_tasks_.empty()) {
    return std::nullopt;
  }

  return scheduled_tasks_.begin()->first;
}

std::vector<std::shared_ptr<ScheduledExecutor::ScheduledTask>>
ScheduledExecutor::FetchTasksToRun() {
  absl::MutexLock lock(&mutex_);
  std::vector<std::shared_ptr<ScheduledTask>> tasks;

  if (shut_down_) {
    return tasks;
  }

  absl::Time now = SystemClock::ElapsedRealtime();
  auto it = scheduled_tasks_.begin();
  while (it != scheduled_tasks_.end()) {
    if (it->first <= now) {
      tasks.push_back(it->second);
    } else {
      break;
    }
    ++it;
  }

  return tasks;
}

}  // namespace windows
}  // namespace nearby
