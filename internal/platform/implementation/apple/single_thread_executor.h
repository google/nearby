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

#ifndef PLATFORM_IMPL_APPLE_SINGLE_THREAD_EXECUTOR_H_
#define PLATFORM_IMPL_APPLE_SINGLE_THREAD_EXECUTOR_H_

#include <pthread.h>

#include <deque>
#include <memory>

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/submittable_executor.h"
#include "internal/platform/runnable.h"

namespace nearby {
namespace apple {

class SingleThreadExecutor : public api::SubmittableExecutor {
 public:
  SingleThreadExecutor();
  ~SingleThreadExecutor() override;
  SingleThreadExecutor(const SingleThreadExecutor&) = delete;
  SingleThreadExecutor& operator=(const SingleThreadExecutor&) = delete;

  void Execute(Runnable&& runnable) override;
  bool DoSubmit(Runnable&& runnable) override;
  void Shutdown() override ABSL_LOCKS_EXCLUDED(join_mutex_);

 private:
  struct WorkerState {
    absl::Mutex mutex;
    std::deque<Runnable> queue ABSL_GUARDED_BY(mutex);
    bool shutting_down ABSL_GUARDED_BY(mutex) = false;
  };

  static void* ThreadMain(void* arg);
  static void RunWorkerLoop(WorkerState& state)
      ABSL_LOCKS_EXCLUDED(state.mutex);

  std::shared_ptr<WorkerState> state_;
  pthread_t thread_{};
  bool thread_started_ = false;
  absl::Mutex join_mutex_;
  bool thread_joined_ ABSL_GUARDED_BY(join_mutex_) = false;
};

}  // namespace apple
}  // namespace nearby

#endif  // PLATFORM_IMPL_APPLE_SINGLE_THREAD_EXECUTOR_H_
