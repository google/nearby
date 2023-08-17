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

#include "internal/platform/implementation/linux/timer_queue.h"

#include "internal/platform/implementation/linux/utils.h"

#include "internal/platform/logging.h"

namespace nearby {
namespace linux {

std::unique_ptr<TimerQueue> TimerQueue::CreateTimerQueue() {
  return std::unique_ptr<TimerQueue>(new TimerQueue);
}

TimerQueue::TimerQueue() {
  thread_ = std::thread([this]() {Run();});
}

TimerQueue::~TimerQueue() {
  DeleteTimerQueueEx(CE_WAITFORCALLBACKS); // NOLINT
  cancelled_ = true;
  thread_.join();
}

void TimerQueue::Run() {
  while(!cancelled_ || !work_.empty()) {
    absl::MutexLock lk(&mutex_);
    empty_ = (work_.empty() ? true : false);
    for (auto &workItem : work_) {
      if (workItem.Finished) {
        if (workItem.Due < std::chrono::steady_clock::now()) {
          work_.erase(workItem);
          break;
        }
      }
      if (workItem.Cancelled) {
        if (workItem.Worker.valid()) {
          if (workItem.Worker.wait_for(std::chrono_literals::operator""ms(0)) == std::future_status::ready) {
            work_.erase(workItem);
            break;
          }
        }
      }
      if (workItem.Due <= std::chrono::steady_clock::now()) {
        if ((workItem.Flags & WT_EXECUTEDEFAULT) == WT_EXECUTEDEFAULT) {
          if ((workItem.Flags & WT_EXECUTEONLYONCE) == WT_EXECUTEONLYONCE) {
            workItem.Worker = std::async(workItem.Callback, workItem.Parameter);
            workItem.Finished = true;
          }
          else {
            if (workItem.Period > std::chrono_literals::operator""ms(0)) {
              workItem.Worker = std::async(workItem.Callback, workItem.Parameter);
              workItem.Finished = true;
              workItem.Due = std::chrono::steady_clock::now() + workItem.Period;
            }
            else {
              workItem.Worker = std::async(workItem.Callback, workItem.Parameter);
              workItem.Finished = true;
            }
          }
        }
        else if ((workItem.Flags & WT_EXECUTEINTIMERTHREAD) == WT_EXECUTEINTIMERTHREAD) {
          if ((workItem.Flags & WT_EXECUTEONLYONCE) == WT_EXECUTEONLYONCE) {
            workItem.Callback(workItem.Parameter);
            workItem.Finished = true;
          }
          else {
            if (workItem.Period > std::chrono_literals::operator""ms(0)) {
              workItem.Callback(workItem.Parameter);
              workItem.Finished = true;
              workItem.Due = std::chrono::steady_clock::now() + workItem.Period;
            }
            else {
              workItem.Callback(workItem.Parameter);
              workItem.Finished = true;
            }
          }
        }
      }
    }
  }
}

absl::StatusOr<uint16_t> TimerQueue::CreateTimerQueueTimer(absl::AnyInvocable<void(void *lpParameter)> Callback, void *Parameter, std::chrono::milliseconds DueTime, std::chrono::milliseconds Period, unsigned long int Flags) {
  TimerWork work;

  work.Callback = std::move(Callback);
  work.WorkId = next_id_;
  work.Flags = Flags;
  next_id_++;
  work.Due = std::chrono::steady_clock::now() + DueTime;
  work.Period = Period;
  work.Parameter = Parameter;
  absl::MutexLock lk(&mutex_);
  work_.insert(std::move(work));
  return next_id_ - 1;
}

absl::Status TimerQueue::DeleteTimerQueueTimer(uint16_t WorkId, unsigned long int CompletionEvent) {
  for (auto &workItem : work_) {
    absl::MutexLock lk(&mutex_);
    if (workItem.WorkId == WorkId) {
      workItem.Cancelled = true;
    }
  }
  return absl::OkStatus();
}

namespace {
  bool check(bool *arg) {
    return *arg;
  }
}  // namespace

absl::Status TimerQueue::DeleteTimerQueueEx(unsigned int long CompletionEvent) {
  switch (CompletionEvent) {
    case CE_IMEDIATERETURN: {
      absl::MutexLock lk(&mutex_);
      for (auto workItem = work_.begin(); workItem != work_.end();) {
        if (workItem->Worker.valid()) {
          workItem->Worker.wait();
        }
        workItem = work_.erase(workItem);
      }
      return absl::OkStatus();
    }
    case CE_WAITFORCALLBACKS: {
      mutex_.Lock();
      for (auto &workItem : work_) {
        workItem.Cancelled = true;
      }
      clearing_ = true;
      mutex_.Await(absl::Condition(check, &empty_));
      mutex_.Unlock();
      return absl::OkStatus();
    }
    default: {
      return absl::InvalidArgumentError("Invalid Completion Event value.");
    }
  }
}

}  // namespace linux
}  // namespace nearby
