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

#ifndef PLATFORM_PUBLIC_SINGLE_THREAD_EXECUTOR_H_
#define PLATFORM_PUBLIC_SINGLE_THREAD_EXECUTOR_H_

#include <memory>
#include <string>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/base/thread_annotations.h"
#include "absl/time/time.h"
#include "internal/platform/implementation/platform.h"
#include "internal/platform/implementation/single_thread_executor.h"
#include "internal/platform/lockable.h"
#include "internal/platform/monitored_runnable.h"
#include "internal/platform/runnable.h"
#include "internal/platform/thread_check_runnable.h"

namespace nearby {

// An Executor that uses a single worker thread operating off an unbounded
// queue.
//
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/Executors.html#newSingleThreadExecutor--
class ABSL_LOCKABLE SingleThreadExecutor : public api::SingleThreadExecutor,
                                           public Lockable {
 public:
  using Platform = api::ImplementationPlatform;
  SingleThreadExecutor() : impl_(Platform::CreateSingleThreadExecutor()) {}
  SingleThreadExecutor(const SingleThreadExecutor&) = delete;
  SingleThreadExecutor& operator=(const SingleThreadExecutor&) = delete;
  ~SingleThreadExecutor() override { Shutdown(); }

  virtual bool Execute(const std::string& name, Runnable&& runnable) {
    return impl_->Execute(MakeMonitoredRunnable(
        name, MakeThreadCheckRunnable(this, std::move(runnable))));
  }

  bool Execute(Runnable&& runnable) override {
    return impl_->Execute(MakeMonitoredRunnable(
        MakeThreadCheckRunnable(this, std::move(runnable))));
  }

  // Schedules `runnable` to execute on the worker thread after `delay` (clamped
  // to `INT_MAX` milliseconds). A zero or negative `delay` is equivalent to
  // `Execute(std::move(runnable))`. Returns `true` if `runnable` was
  // scheduled; returns `false` if `runnable` is empty, the executor has been
  // shut down, or scheduling fails. Pending delayed tasks that have not yet
  // started are cancelled on `Shutdown()` or destruction.
  bool DelayedExecute(absl::Duration delay, Runnable&& runnable) override {
    if (delay <= absl::ZeroDuration()) {
      return Execute(std::move(runnable));
    }
    return impl_->DelayedExecute(
        delay, MakeThreadCheckRunnable(this, std::move(runnable)));
  }

  void Shutdown() override { impl_->Shutdown(); }

  bool IsRunningOnExecutor() const override {
    return impl_->IsRunningOnExecutor();
  }

 protected:
  explicit SingleThreadExecutor(
      absl_nonnull std::unique_ptr<api::SingleThreadExecutor> impl)
      : impl_(std::move(impl)) {}

 private:
  const absl_nonnull std::unique_ptr<api::SingleThreadExecutor> impl_;
};

}  // namespace nearby

#endif  // PLATFORM_PUBLIC_SINGLE_THREAD_EXECUTOR_H_
