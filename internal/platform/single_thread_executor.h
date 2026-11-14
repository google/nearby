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

#include <cstdint>
#include <memory>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "internal/platform/implementation/platform.h"
#include "internal/platform/submittable_executor.h"
#include "internal/platform/timer.h"

namespace nearby {

// An Executor that uses a single worker thread operating off an unbounded
// queue.
//
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/Executors.html#newSingleThreadExecutor--
class ABSL_LOCKABLE SingleThreadExecutor : public SubmittableExecutor {
 public:
  using Platform = api::ImplementationPlatform;
  SingleThreadExecutor()
      : SubmittableExecutor(Platform::CreateSingleThreadExecutor()) {}
  SingleThreadExecutor(const SingleThreadExecutor&) = delete;
  SingleThreadExecutor& operator=(const SingleThreadExecutor&) = delete;
  ~SingleThreadExecutor() override;

  bool DelayedExecute(absl::Duration delay, absl::AnyInvocable<void()> task)
      ABSL_LOCKS_EXCLUDED(mutex_);

  void Shutdown() override ABSL_LOCKS_EXCLUDED(mutex_);

 protected:
  virtual std::unique_ptr<Timer> CreateTimer();

 private:
  void DoShutdown() ABSL_LOCKS_EXCLUDED(mutex_);

  mutable absl::Mutex mutex_;
  absl::flat_hash_map<uint64_t, std::unique_ptr<Timer>> timers_map_
      ABSL_GUARDED_BY(mutex_);
  bool closed_ ABSL_GUARDED_BY(mutex_) = false;
};

// Moves the object to `executor` and destroys it there.
// This pattern is useful when there are some tasks running on `executor` that
// hold a reference to `object`. The tasks will complete before `object` is
// destroyed.
template <typename T>
void DestroyOnExecutor(T object, SingleThreadExecutor* executor) {
  executor->Execute([object = std::move(object)] {});
}

}  // namespace nearby

#endif  // PLATFORM_PUBLIC_SINGLE_THREAD_EXECUTOR_H_
