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

#ifndef PLATFORM_PUBLIC_SUBMITTABLE_EXECUTOR_H_
#define PLATFORM_PUBLIC_SUBMITTABLE_EXECUTOR_H_

#include <memory>
#include <string>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/executor.h"
#include "internal/platform/implementation/submittable_executor.h"
#include "internal/platform/lockable.h"
#include "internal/platform/monitored_runnable.h"
#include "internal/platform/runnable.h"
#include "internal/platform/thread_check_runnable.h"

namespace nearby {

inline int GetCurrentTid() { return api::GetCurrentTid(); }

// Main interface to be used by platform as a base class for
// - MultiThreadExecutor
// - SingleThreadExecutor
class ABSL_LOCKABLE SubmittableExecutor : public api::Executor,
                                          public Lockable {
 public:
  SubmittableExecutor(const SubmittableExecutor&) = delete;
  SubmittableExecutor& operator=(const SubmittableExecutor&) = delete;
  ~SubmittableExecutor() override { DoShutdown(); }
  virtual void Execute(const std::string& name, Runnable&& runnable)
      ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock(mutex_);
    if (impl_)
      impl_->Execute(MonitoredRunnable(
          name, ThreadCheckRunnable(this, std::move(runnable))));
  }

  void Execute(Runnable&& runnable) ABSL_LOCKS_EXCLUDED(mutex_) override {
    absl::MutexLock lock(mutex_);
    if (impl_)
      impl_->Execute(
          MonitoredRunnable(ThreadCheckRunnable(this, std::move(runnable))));
  }

  void Shutdown() ABSL_LOCKS_EXCLUDED(mutex_) override { DoShutdown(); }

 protected:
  explicit SubmittableExecutor(std::unique_ptr<api::SubmittableExecutor> impl)
      : impl_(std::move(impl)) {}

 private:
  void DoShutdown() ABSL_LOCKS_EXCLUDED(mutex_) {
    std::unique_ptr<api::SubmittableExecutor> executor = ReleaseExecutor();
    if (executor) {
      executor->Shutdown();
    }
  }

  std::unique_ptr<api::SubmittableExecutor> ReleaseExecutor()
      ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock(mutex_);
    return std::move(impl_);
  }

  mutable absl::Mutex mutex_;
  std::unique_ptr<api::SubmittableExecutor> ABSL_GUARDED_BY(mutex_) impl_;
};

}  // namespace nearby

#endif  // PLATFORM_PUBLIC_SUBMITTABLE_EXECUTOR_H_
