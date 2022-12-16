// Copyright 2022 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_INTERNAL_PUBLIC_TASK_RUNNER_IMPL_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_INTERNAL_PUBLIC_TASK_RUNNER_IMPL_H_

#include <functional>
#include <memory>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "fastpair/internal/public/task_runner.h"
#include "fastpair/internal/public/timer.h"
#include "internal/platform/multi_thread_executor.h"

namespace location {
namespace nearby {
namespace fastpair {

class TaskRunnerImpl : public TaskRunner {
 public:
  explicit TaskRunnerImpl(uint32_t runner_count);
  ~TaskRunnerImpl() override;

  bool PostTask(std::function<void()> task) override;
  bool PostDelayedTask(absl::Duration delay,
                       std::function<void()> task) override
      ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  uint64_t GenerateId();

  mutable absl::Mutex mutex_;
  std::unique_ptr<::location::nearby::MultiThreadExecutor> executor_;
  absl::flat_hash_map<uint64_t, std::unique_ptr<Timer>> timers_map_
      ABSL_GUARDED_BY(mutex_);
};

}  // namespace fastpair
}  // namespace nearby
}  // namespace location

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_INTERNAL_PUBLIC_TASK_RUNNER_IMPL_H_
