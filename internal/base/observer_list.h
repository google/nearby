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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_BASE_OBSERVER_LIST_H_
#define THIRD_PARTY_NEARBY_INTERNAL_BASE_OBSERVER_LIST_H_

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_set.h"
#include "absl/synchronization/mutex.h"

namespace nearby {

template <class ObserverType>
class ObserverList {
 public:
  using iterator = typename absl::flat_hash_set<ObserverType*>::iterator;
  using const_iterator =
      typename absl::flat_hash_set<ObserverType*>::const_iterator;

  void AddObserver(ObserverType* observer) ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock(&mutex_);
    observers_.emplace(observer);
  }

  void RemoveObserver(ObserverType* observer) ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock(&mutex_);
    observers_.erase(observer);
  }

  bool HasObserver(ObserverType* observer) ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock(&mutex_);
    return observers_.contains(observer);
  }

  void Clear() ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock(&mutex_);
    observers_.clear();
  }

  bool empty() const ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock(&mutex_);
    return observers_.empty();
  }

  int size() const ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock(&mutex_);
    return observers_.size();
  }

  // Supports iterators
  iterator begin() ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock(&mutex_);
    return observers_.begin();
  }
  iterator end() ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock(&mutex_);
    return observers_.end();
  }
  const_iterator begin() const ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock(&mutex_);
    return observers_.begin();
  }
  const_iterator end() const ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock(&mutex_);
    return observers_.end();
  }

 private:
  mutable absl::Mutex mutex_;
  absl::flat_hash_set<ObserverType*> observers_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_BASE_OBSERVER_LIST_H_
