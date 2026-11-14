// Copyright 2026 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_TEST_TEST_SINGLE_THREAD_EXECUTOR_H_
#define THIRD_PARTY_NEARBY_INTERNAL_TEST_TEST_SINGLE_THREAD_EXECUTOR_H_

#include <atomic>
#include <memory>

#include "absl/base/thread_annotations.h"
#include "absl/time/time.h"
#include "internal/platform/single_thread_executor.h"
#include "internal/platform/timer.h"
#include "internal/test/fake_clock.h"

namespace nearby {

// A SingleThreadExecutor for unit tests that runs tasks on a real worker thread
// while driving DelayedExecute timers through FakeClock.
class ABSL_LOCKABLE TestSingleThreadExecutor : public SingleThreadExecutor {
 public:
  explicit TestSingleThreadExecutor(FakeClock* clock) : clock_(clock) {}
  TestSingleThreadExecutor(const TestSingleThreadExecutor&) = delete;
  TestSingleThreadExecutor& operator=(const TestSingleThreadExecutor&) = delete;
  ~TestSingleThreadExecutor() override;

  void Shutdown() override;

  // Waits for all queued tasks on the executor thread to complete.
  void Sync();

  // Waits up to `timeout` for all queued tasks on the executor thread to
  // complete.
  bool SyncWithTimeout(absl::Duration timeout);

 protected:
  std::unique_ptr<Timer> CreateTimer() override;

 private:
  FakeClock* const clock_;
  std::atomic<bool> closed_ = false;
};

}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_TEST_TEST_SINGLE_THREAD_EXECUTOR_H_
