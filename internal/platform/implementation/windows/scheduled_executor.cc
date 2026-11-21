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

#include <atomic>
#include <cassert>
#include <cstdint>
#include <memory>
#include <utility>

#include "absl/time/time.h"
#include "internal/platform/implementation/cancelable.h"
#include "internal/platform/implementation/windows/thread_pool.h"
#include "internal/platform/logging.h"
#include "internal/platform/runnable.h"

namespace nearby {
namespace windows {

ScheduledExecutor::ScheduledExecutor() : ScheduledExecutor(1) {}

ScheduledExecutor::ScheduledExecutor(int32_t max_concurrency)
    : thread_pool_(ThreadPool::Create(
          max_concurrency > 0 ? static_cast<uint32_t>(max_concurrency) : 0)) {
  assert(max_concurrency >= 1);
  assert(thread_pool_ != nullptr);
}

ScheduledExecutor::~ScheduledExecutor() { Shutdown(); }

// Cancelable is kept both in the executor context, and in the caller context.
// We want Cancelable to live until both caller and executor are done with it.
// Exclusive ownership model does not work for this case;
// using std:shared_ptr<> instead of std::unique_ptr<>.
std::shared_ptr<api::Cancelable> ScheduledExecutor::Schedule(
    Runnable&& runnable, absl::Duration duration) {
  if (shut_down_.load(std::memory_order_acquire)) {
    LOG(ERROR) << __func__ << ": Attempt to Schedule on a shut down executor.";

    return nullptr;
  }

  return task_scheduler_.Schedule(std::move(runnable), duration);
}

bool ScheduledExecutor::Execute(Runnable&& runnable) {
  if (shut_down_.load(std::memory_order_acquire)) {
    VLOG(1) << __func__ << ": Attempt to Execute on a shut down executor.";
    return false;
  }

  if (runnable == nullptr) {
    LOG(ERROR) << __func__ << ": Runnable was null.";
    return false;
  }

  if (thread_pool_ == nullptr) {
    return false;
  }

  return thread_pool_->Run(std::move(runnable));
}

void ScheduledExecutor::Shutdown() {
  if (!shut_down_.exchange(true, std::memory_order_acq_rel)) {
    if (thread_pool_ != nullptr) {
      thread_pool_->ShutDown();
    }
    task_scheduler_.Shutdown();
    return;
  }
  VLOG(1) << __func__ << ": Attempt to Shutdown on a shut down executor.";
}
}  // namespace windows
}  // namespace nearby
