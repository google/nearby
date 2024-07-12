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

#ifndef PLATFORM_PUBLIC_ARRAY_BLOCKING_QUEUE_H_
#define PLATFORM_PUBLIC_ARRAY_BLOCKING_QUEUE_H_

#include <cstddef>
#include <optional>
#include <queue>

#include "internal/platform/condition_variable.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex.h"
#include "internal/platform/mutex_lock.h"

namespace nearby {

/**
 * Payload from different services/clients will be put into an
 * ArrayBlockingQueue before sending to ensure each client has equal chance to
 * send its data. Since C++ doesn't provide ArrayBlockingQueue as Java, we
 * implement one here.
 */
template <typename T>
class ArrayBlockingQueue {
 public:
  explicit ArrayBlockingQueue(size_t capacity) : capacity_(capacity) {}

  void Put(const T& value) {
    MutexLock lock(&queue_mutex_);
    if (queue_.size() >= capacity_) {
      has_space_.Wait();
    }
    queue_.push(value);
    NEARBY_LOGS(INFO) << "ArrayBlockingQueue::Put()";
    has_data_.Notify();
  }

  T Take() {
    MutexLock lock(&queue_mutex_);
    if (queue_.empty()) {
      has_data_.Wait();
    }
    T front = queue_.front();
    queue_.pop();
    NEARBY_LOGS(INFO) << "ArrayBlockingQueue::Take()";
    has_space_.Notify();
    return front;
  }

  bool TryPut(const T& value) {
    MutexLock lock(&queue_mutex_);
    if (queue_.size() < capacity_) {
      queue_.push(value);
      has_data_.Notify();
      return true;
    }
    return false;
  }

  // Returns std::nullopt if the queue is empty.
  std::optional<T> TryTake() {
    MutexLock lock(&queue_mutex_);
    if (!queue_.empty()) {
      T front = queue_.front();
      queue_.pop();
      has_space_.Notify();
      return front;
    }
    return std::nullopt;
  }

  size_t Size() const {
    MutexLock lock(&queue_mutex_);
    return queue_.size();
  }

  bool Empty() const {
    MutexLock lock(&queue_mutex_);
    return queue_.empty();
  }

 private:
  std::queue<T> queue_;
  mutable Mutex queue_mutex_;
  ConditionVariable has_data_{&queue_mutex_};
  ConditionVariable has_space_{&queue_mutex_};
  const size_t capacity_;
};

}  // namespace nearby

#endif  // PLATFORM_PUBLIC_ARRAY_BLOCKING_QUEUE_H_
