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

#ifndef PLATFORM_IMPL_G3_SCHEDULED_EXECUTOR_H_
#define PLATFORM_IMPL_G3_SCHEDULED_EXECUTOR_H_

#include <atomic>
#include <memory>

#include "absl/time/clock.h"
#include "internal/platform/implementation/cancelable.h"
#include "internal/platform/implementation/scheduled_executor.h"
#include "internal/platform/runnable.h"
#include "internal/platform/implementation/g3/single_thread_executor.h"
#include "nisaba/port/thread_pool.h"

namespace nearby {
namespace g3 {

// An Executor that reuses a fixed number of threads operating off a shared
// unbounded queue.
class ScheduledExecutor final : public api::ScheduledExecutor {
 public:
  ScheduledExecutor() = default;
  ~ScheduledExecutor() override { executor_.Shutdown(); }

  void Execute(Runnable&& runnable) override {
    executor_.Execute(std::move(runnable));
  }
  std::shared_ptr<api::Cancelable> Schedule(Runnable&& runnable,
                                            absl::Duration delay) override;
  void Shutdown() override { executor_.Shutdown(); }

 private:
  SingleThreadExecutor executor_;
};

}  // namespace g3
}  // namespace nearby

#endif  // PLATFORM_IMPL_G3_SCHEDULED_EXECUTOR_H_
