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

#include "platform/impl/windows/scheduled_executor.h"

#include "platform/impl/windows/cancelable.h"
#include "platform/public/logging.h"

namespace location {
namespace nearby {
namespace windows {

class ScheduledExecutorException : public std::runtime_error {
 public:
  ScheduledExecutorException() : std::runtime_error("") {}
  ScheduledExecutorException(const std::string& message)
      : std::runtime_error(message) {}
  virtual const char* what() const throw() {
    return "WaitableTimer creation failed";
  }
};

class TimerData {
 public:
  TimerData(ScheduledExecutor* scheduledExecutor,
            std::function<void()> runnable, HANDLE waitableTimer)
      : scheduled_executor_(scheduledExecutor),
        runnable_(std::move(runnable)),
        waitable_timer_handle_(waitableTimer) {}

  ScheduledExecutor* GetScheduledExecutor() { return scheduled_executor_; }
  std::function<void()> GetRunnable() { return runnable_; }
  HANDLE GetWaitableTimerHandle() { return waitable_timer_handle_; }

 private:
  ScheduledExecutor* scheduled_executor_;
  std::function<void()> runnable_;
  HANDLE waitable_timer_handle_;
};

void WINAPI ScheduledExecutor::_TimerProc(LPVOID argToCompletionRoutine,
                                          DWORD dwTimerLowValue,
                                          DWORD dwTimerHighValue) {
  TimerData* timerData;
  DWORD threadId = GetCurrentThreadId();

  _ASSERT(argToCompletionRoutine != NULL);
  if (NULL == argToCompletionRoutine) {
    NEARBY_LOGS(ERROR)
        << __func__
        << ": TimerProc argument argToCompletionRoutine was null.";

    return;
  }

  timerData = static_cast<TimerData*>(argToCompletionRoutine);
  timerData->GetScheduledExecutor()->Execute(timerData->GetRunnable());

  // Get the waitable timer and destroy it
  CloseHandle(timerData->GetWaitableTimerHandle());
  free(timerData);
  return;
}

ScheduledExecutor::ScheduledExecutor()
    : executor_(std::make_unique<nearby::windows::Executor>()),
      shut_down_(false) {}

// Cancelable is kept both in the executor context, and in the caller context.
// We want Cancelable to live until both caller and executor are done with it.
// Exclusive ownership model does not work for this case;
// using std:shared_ptr<> instead of std::unique_ptr<>.
std::shared_ptr<api::Cancelable> ScheduledExecutor::Schedule(
    Runnable&& runnable, absl::Duration duration) {
  if (shut_down_) {
    NEARBY_LOGS(ERROR)
        << __func__
        << ": Attempt to Schedule on a shut down executor.";

    return nullptr;
  }

  // Create the waitable timer
  // Create a name for this timer
  // TODO: (jfcarroll) construct a timer name based on ??
  char buffer[TIMER_NAME_BUFFER_SIZE];

  snprintf(buffer, TIMER_NAME_BUFFER_SIZE, "PID:%ld", GetCurrentProcessId());

  HANDLE waitableTimer = CreateWaitableTimerA(NULL, true, buffer);

  if (waitableTimer == NULL) {
    throw ScheduledExecutorException("WaitableTimer creation failed");
  }

  waitable_timers_.push_back(waitableTimer);

  // Create the delay value - due time
  LARGE_INTEGER dueTime;
  dueTime.QuadPart = -(absl::ToChronoNanoseconds(duration).count() / 100);

  TimerData* timerData = new TimerData(this, runnable, waitableTimer);

  BOOL result = SetWaitableTimer(waitableTimer, &dueTime, 0, _TimerProc,
                                 timerData, false);

  if (result == 0) {
    NEARBY_LOGS(ERROR) << "Error: " << __func__ << ": Failed to set the timer.";
    return nullptr;
  }

  return std::make_shared<nearby::windows::Cancelable>(waitableTimer);
}

// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/Executor.html#execute-java.lang.Runnable-
void ScheduledExecutor::Execute(Runnable&& runnable) {
  if (shut_down_) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Attempt to Execute on a shut down executor.";
    return;
  }

  executor_->Execute(std::move(runnable));
}

// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/ExecutorService.html#shutdown--
void ScheduledExecutor::Shutdown() {
  if (!shut_down_) {
    shut_down_ = true;
    executor_->Shutdown();
    return;
  }
  NEARBY_LOGS(ERROR) << __func__
                     << ": Attempt to Shutdown on a shut down executor.";
}
}  // namespace windows
}  // namespace nearby
}  // namespace location
