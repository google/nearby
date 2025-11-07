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

#ifndef THIRD_PARTY_NEARBY_SHARING_WORKER_QUEUE_H_
#define THIRD_PARTY_NEARBY_SHARING_WORKER_QUEUE_H_

#include <atomic>
#include <queue>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/functional/any_invocable.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/task_runner.h"
#include "sharing/internal/public/logging.h"

namespace nearby::sharing {

// A queue that runs a callback on a worker thread to handle queued items.
//
// The callback is edge triggered, i.e. it will only be scheduled when the queue
// changes from empty to non-empty.  This object guarantees that only 1 callback
// is scheduled at a time until the `ReadAll` method is called which resets the
// scheduling state.
//
// This class is thread-safe.
template <typename T>
class WorkerQueue {
 public:
  explicit WorkerQueue(TaskRunner* task_runner) : task_runner_(task_runner) {}

  ~WorkerQueue() { Stop(); }

  // Starts the queue.  `callback` will be called on the worker thread when
  // there are items in the queue.
  // Returns true if the queue is started successfully.
  // Returns false if the queue is already started or stopped.  Queue cannot be
  // restarted.
  bool Start(absl::AnyInvocable<void()> callback) {
    if (is_started_.exchange(true)) {
      LOG(ERROR) << "WorkerQueue is already started.";
      return false;
    }
    if (is_stopped_) {
      LOG(ERROR) << "WorkerQueue is already stopped, cannot restart.";
      return false;
    }
    callback_ = std::move(callback);
    {
      absl::MutexLock lock(mutex_);
      if (!queue_.empty()) {
        ScheduleCallback();
      }
    }
    return true;
  }

  // Stops the queue.  No new callback will be scheduled.
  void Stop() {
    bool already_stopped = is_stopped_.exchange(true);
    if (already_stopped || !is_started_) {
      return;
    }
    is_scheduled_ = true;
  }

  // Queues an item to be processed by the callback.
  // Callback are edge triggered.
  void Queue(T item) {
    absl::MutexLock lock(mutex_);
    queue_.push(std::move(item));
    ScheduleCallback();
  }

  // Returns all the items in the queue and clears the queue.
  // This resets the callback scheduling state and new callbacks will be
  // scheduled when new items are queued.
  std::queue<T> ReadAll() {
    is_scheduled_ = false;
    absl::MutexLock lock(mutex_);
    std::queue<T> queue;
    queue.swap(queue_);
    return queue;
  }

 private:
  void ScheduleCallback() {
    // Skip if not started or stopped
    if (!is_started_ || is_stopped_) {
      return;
    }
    if (is_scheduled_.exchange(true)) {
      VLOG(1) << "Already scheduled";
      // Already scheduled.
      return;
    }
    VLOG(1) << "Scheduling callback";
    task_runner_->PostTask([this]() {
      if (is_stopped_) {
        return;
      }
      callback_();
    });
  }

  TaskRunner* const task_runner_ = nullptr;
  absl::AnyInvocable<void()> callback_;
  // Tracks whether Start() has been called.
  std::atomic<bool> is_started_ = false;
  // Tracks whether Stop() has been called.
  std::atomic<bool> is_stopped_ = false;
  absl::Mutex mutex_;
  std::queue<T> queue_ ABSL_GUARDED_BY(mutex_);
  // This is used track whether the callback is already scheduled so as to avoid
  // scheduling multiple callbacks.
  // `is_scheduled_` must be false if is_started_ is false.
  std::atomic<bool> is_scheduled_ = false;
};

}  // namespace nearby::sharing

#endif  // THIRD_PARTY_NEARBY_SHARING_WORKER_QUEUE_H_
