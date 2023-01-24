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

#ifndef PLATFORM_PUBLIC_TASK_RUNNER_IMPL_H_
#define PLATFORM_PUBLIC_TASK_RUNNER_IMPL_H_

// Kludge: third_party/leveldb introduces macro UNICODE to all its dependents
// and causes CreateMutex to expand to CreateMutexW.
#if defined(_WIN32) && defined(UNICODE)
#pragma push_macro("UNICODE")
#define UNICODE_PUSHED
#undef UNICODE
#endif

#include <memory>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/multi_thread_executor.h"
#include "internal/platform/task_runner.h"
#include "internal/platform/timer.h"

namespace nearby {

class TaskRunnerImpl : public TaskRunner {
 public:
  explicit TaskRunnerImpl(uint32_t runner_count);
  ~TaskRunnerImpl() override;

  bool PostTask(absl::AnyInvocable<void()> task) override;
  bool PostDelayedTask(absl::Duration delay,
                       absl::AnyInvocable<void()> task) override
      ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  uint64_t GenerateId();

  mutable absl::Mutex mutex_;
  std::unique_ptr<::nearby::MultiThreadExecutor> executor_;
  absl::flat_hash_map<uint64_t, std::unique_ptr<Timer>> timers_map_
      ABSL_GUARDED_BY(mutex_);
};

}  // namespace nearby

#ifdef UNICODE_PUSHED
#pragma pop_macro("UNICODE")
#undef UNICODE_PUSHED
#endif

#endif  // PLATFORM_PUBLIC_TASK_RUNNER_IMPL_H_
