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

#include <algorithm>
#include <memory>
#include <utility>

#include "absl/time/time.h"
#include "internal/platform/logging.h"

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
  if (shut_down_) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Attempt to Schedule on a shut down executor.";

    return nullptr;
  }

  // Cleans completed tasks
  std::remove_if(
      scheduled_tasks_.begin(), scheduled_tasks_.end(),
      [](std::shared_ptr<ScheduledTask>& task) { return task->IsDone(); });

  std::shared_ptr<ScheduledTask> task =
      std::make_shared<ScheduledTask>(std::move(runnable), duration);

  scheduled_tasks_.push_back(task);
  executor_->Execute([task]() { task->Start(); });
  return task;
}

void ScheduledExecutor::Execute(Runnable&& runnable) {
  if (shut_down_) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Attempt to Execute on a shut down executor.";
    return;
  }

  executor_->Execute(std::move(runnable));
}

void ScheduledExecutor::Shutdown() {
  if (!shut_down_) {
    shut_down_ = true;
    for (auto& task : scheduled_tasks_) {
      task->Cancel();
    }

    scheduled_tasks_.clear();
    executor_->Shutdown();
    return;
  }
  NEARBY_LOGS(ERROR) << __func__
                     << ": Attempt to Shutdown on a shut down executor.";
}
}  // namespace windows
}  // namespace nearby
