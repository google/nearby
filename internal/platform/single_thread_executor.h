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
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/notification.h"
#include "internal/platform/implementation/executor.h"
#include "internal/platform/implementation/platform.h"
#include "internal/platform/implementation/submittable_executor.h"
#include "internal/platform/submittable_executor.h"

namespace nearby {

// An Executor that uses a single worker thread operating off an unbounded
// queue.
//
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/Executors.html#newSingleThreadExecutor--
class ABSL_LOCKABLE SingleThreadExecutor : public SubmittableExecutor {
 public:
  using Platform = api::ImplementationPlatform;

  SingleThreadExecutor()
      : SubmittableExecutor(Platform::CreateSingleThreadExecutor()) {
#if !defined(NEARBY_CHROMIUM)
    absl::Notification ready;
    if (SubmitInternal([this, &ready]() {
          thread_id_ = api::GetCurrentThreadId();
          ready.Notify();
        })) {
      ready.WaitForNotification();
    }
#endif
  }
  ~SingleThreadExecutor() override = default;
  SingleThreadExecutor(SingleThreadExecutor&& other) noexcept
      : SubmittableExecutor(std::move(other)),
        thread_id_(std::exchange(other.thread_id_, 0)) {}
  SingleThreadExecutor& operator=(SingleThreadExecutor&& other) noexcept {
    if (this != &other) {
      SubmittableExecutor::operator=(std::move(other));
      thread_id_ = std::exchange(other.thread_id_, 0);
    }
    return *this;
  }

  // Returns the native OS thread ID of the worker thread backing this executor.
  virtual ThreadId GetThreadId() const { return thread_id_; }

 protected:
  SingleThreadExecutor(std::unique_ptr<api::SubmittableExecutor> impl,
                       ThreadId thread_id)
      : SubmittableExecutor(std::move(impl)), thread_id_(thread_id) {}

 private:
  ThreadId thread_id_ = 0;
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
