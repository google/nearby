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
#include <utility>

#include "absl/time/time.h"
#include "internal/flags/nearby_flags.h"
#include "internal/platform/flags/nearby_platform_feature_flags.h"
#include "internal/platform/implementation/cancelable.h"
#include "internal/platform/logging.h"
#include "internal/platform/runnable.h"

namespace nearby {
namespace windows {

ScheduledExecutor::ScheduledExecutor()
    : executor_(std::make_unique<nearby::windows::Executor>()),
      shut_down_(false),
      use_task_scheduler_(NearbyFlags::GetInstance().GetBoolFlag(
          platform::config_package_nearby::nearby_platform_feature::
              kEnableTaskScheduler)) {}

// Cancelable is kept both in the executor context, and in the caller context.
// We want Cancelable to live until both caller and executor are done with it.
// Exclusive ownership model does not work for this case;
// using std:shared_ptr<> instead of std::unique_ptr<>.
std::shared_ptr<api::Cancelable> ScheduledExecutor::Schedule(
    Runnable&& runnable, absl::Duration duration) {
  if (use_task_scheduler_) {
    if (shut_down_) {
      LOG(ERROR) << __func__
                 << ": Attempt to Schedule on a shut down executor.";

      return nullptr;
    }
    return task_scheduler_.Schedule(std::move(runnable), duration);
  } else {
    if (shut_down_) {
      LOG(ERROR) << __func__
                 << ": Attempt to Schedule on a shut down executor.";

      return nullptr;
    }

    // Cleans completed tasks
    auto it = scheduled_tasks_.begin();
    while (it != scheduled_tasks_.end()) {
      if ((*it)->IsDone()) {
        it = scheduled_tasks_.erase(it);
      } else {
        ++it;
      }
    }

    std::shared_ptr<ScheduledTask> task =
        std::make_shared<ScheduledTask>(std::move(runnable), duration);

    scheduled_tasks_.push_back(task);
    executor_->Execute([task]() { task->Start(); });
    return task;
  }
}

void ScheduledExecutor::Execute(Runnable&& runnable) {
  if (shut_down_) {
    LOG(ERROR) << __func__ << ": Attempt to Execute on a shut down executor.";
    return;
  }

  executor_->Execute(std::move(runnable));
}

void ScheduledExecutor::Shutdown() {
  if (use_task_scheduler_) {
    if (!shut_down_) {
      shut_down_ = true;
      executor_->Shutdown();
      task_scheduler_.Shutdown();
      return;
    }
  } else {
    if (!shut_down_) {
      shut_down_ = true;
      for (auto& task : scheduled_tasks_) {
        task->Cancel();
      }

      scheduled_tasks_.clear();
      executor_->Shutdown();
      return;
    }
  }
  LOG(ERROR) << __func__ << ": Attempt to Shutdown on a shut down executor.";
}
}  // namespace windows
}  // namespace nearby
