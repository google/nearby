// Copyright 2021 Google LLC
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

#ifndef PLATFORM_IMPL_LINUX_TIMER_QUEUE_H_
#define PLATFORM_IMPL_LINUX_TIMER_QUEUE_H_

#include <memory>
#include <functional>
#include <thread>
#include <chrono>
#include <set>
#include <future>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/functional/any_invocable.h"
#include "absl/functional/function_ref.h"
#include "absl/synchronization/mutex.h"
#include "absl/base/thread_annotations.h"

namespace nearby {
namespace linux {

/*
 * This class represents a a que for timers. This class is built to try to replicate the Windows Timer-Queue functionality.
 * https://learn.microsoft.com/en-us/windows/win32/api/threadpoollegacyapiset/nf-threadpoollegacyapiset-createtimerqueue
 */
class TimerQueue {
public:
  /*
   * Parameters to be used with the Timer Queue Timer.
   * See https://learn.microsoft.com/en-us/windows/win32/api/threadpoollegacyapiset/nf-threadpoollegacyapiset-createtimerqueuetimer
   */
  static constexpr unsigned long int WT_EXECUTEDEFAULT = 1 << 0;
  static constexpr unsigned long int WT_EXECUTEINTIMERTHREAD = 1 << 1;
  static constexpr unsigned long int WT_EXECUTEINIOTHREAD = 1 << 2; // Unused
  static constexpr unsigned long int WT_EXECUTEINPERSISTENTTHREAD = 1 << 3; // Unused
  static constexpr unsigned long int WT_EXECUTELONGFUNCTION = 1 << 4; // Unused
  static constexpr unsigned long int WT_EXECUTEONLYONCE = 1 << 5;
  static constexpr unsigned long int WT_TRANSFER_IMPERSONATION = 1 << 6; // Unused
  static constexpr unsigned long int CE_WAITFORCALLBACKS = 1 << 0; // Equivalent to `INVALID_HANDLE_VALUE` when passing for a CompletionEvent
  static constexpr unsigned long int CE_IMEDIATERETURN = 1 << 1; //  Equivalent to `NULL` when passing for a CompletionEvent
  // Creates a new timer queue for the user to use.
  static std::unique_ptr<TimerQueue> CreateTimerQueue();

  // Returns a thread unique identifier that the timer is running on if it is successful. Status if not.
  absl::StatusOr<uint16_t> CreateTimerQueueTimer(absl::AnyInvocable<void(void *lpParameter)> Callback, void *Parameter, std::chrono::milliseconds DueTime, std::chrono::milliseconds Period, unsigned long int Flags = WT_EXECUTEDEFAULT);

  // Removes a timer based on the thread id the timer is running on.
  absl::Status DeleteTimerQueueTimer(uint16_t WorkId, unsigned long int CompletionEvent = CE_WAITFORCALLBACKS);

  // Removes all timers in the timer queue
  absl::Status DeleteTimerQueueEx(unsigned long int CompletionEvent = CE_WAITFORCALLBACKS);

  ~TimerQueue();

private:
  TimerQueue();

  void Run();

  uint16_t next_id_;

  // A struct that represents a timer of work that needs to be done.
  struct TimerWork {
    mutable std::chrono::time_point<std::chrono::steady_clock> Due;
    mutable absl::AnyInvocable<void(void *lpParameter)> Callback;
    mutable std::chrono::milliseconds Period;
    mutable void *Parameter = nullptr;
    mutable long int Flags;
    mutable std::future<void> Worker;
    mutable std::atomic_bool Cancelled = false;
    mutable std::atomic_bool Finished = false;
    uint16_t WorkId = 0;

    // Useful for time comparisons on other work items
    bool operator<(const TimerWork &other) const {
      return Due < other.Due;
    }
    bool operator>(const TimerWork &other) const {
      return Due > other.Due;
    }
    bool operator==(const TimerWork &other) const {
      return Due == other.Due;
    }
    bool operator!=(const TimerWork &other) const {
      return !operator==(other);
    }
    bool operator<=(const TimerWork &other) const {
      return (operator<(other) || operator==(other));
    }
    bool operator>=(const TimerWork &other) const {
      return (operator>(other) || operator==(other));
    }
  };

  std::set<TimerWork> work_;
  std::vector<std::thread> threads_;

  std::thread thread_;

  bool cancelled_;
  bool clearing_;
  bool empty_;

  absl::CondVar condvar_;
  absl::Mutex mutex_;
};

}  // namespace linux
}  // namespace nearby

#endif  // PLATFORM_IMPL_LINUX_TIMER_QUEUE_H_
