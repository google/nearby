// Copyright 2024 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_SHARING_THREAD_TIMER_H_
#define THIRD_PARTY_NEARBY_SHARING_THREAD_TIMER_H_

#include <atomic>
#include <cstdint>
#include <string>

#include "absl/functional/any_invocable.h"
#include "absl/time/time.h"
#include "internal/platform/task_runner.h"

namespace nearby::sharing {

// A one shot timer that runs a task on the |task_runner| thread on expiration.
//
// The timer is started when the object is created. The timer can be cancelled
// by calling |Cancel()|.  If |Cancel()| is called from the |task_runner|
// thread before expiration, the task will not be run.  If |Cancel()| is called
// from a different thread, an inflight task may continue until completion.
//
// This class is thread-safe.
class ThreadTimer {
 public:
  explicit ThreadTimer(TaskRunner& task_runner, std::string name,
                       absl::Duration delay, absl::AnyInvocable<void()> task);
  ~ThreadTimer();

  void Cancel();
  bool IsRunning();

 private:
  const std::string name_;
  // The timer state is dependent on the order of expiration and cancellation.
  // The |run_cnt_| is used to track what order these tasks occured.
  // The task that runs last will be responsible for deleting the |run_cnt_|.
  std::atomic<int8_t>* run_cnt_ = nullptr;
};

}  // namespace nearby::sharing

#endif  // THIRD_PARTY_NEARBY_SHARING_THREAD_TIMER_H_
