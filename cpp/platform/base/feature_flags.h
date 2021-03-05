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

#ifndef PLATFORM_BASE_FEATURE_FLAGS_H_
#define PLATFORM_BASE_FEATURE_FLAGS_H_

#include "absl/synchronization/mutex.h"

namespace location {
namespace nearby {

// Global flags that control feature availability. This may be used to gating
// features in development, QA testing and releasing.
class FeatureFlags {
 public:
  // Holds for all the feature flags.
  struct Flags {
    bool enable_cancellation_flag = false;
    bool enable_async_bandwidth_upgrade = true;
    // Let endpoint_manager erase deleted endpoint from endpoints_ inside
    // function RemoveEndpoint.
    bool endpoint_manager_ensure_workers_terminated_inside_remove = true;
    // If a scheduled runnable is already running, Cancel() will synchronously
    // wait for the task to complete.
    bool cancel_waits_for_running_tasks = true;
  };

  static const FeatureFlags& GetInstance() {
    static const FeatureFlags* instance = new FeatureFlags();
    return *instance;
  }

  const Flags& GetFlags() const ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::ReaderMutexLock lock(&mutex_);
    return flags_;
  }

 private:
  FeatureFlags() = default;

  // MediumEnvironment is testing uitl class. Use friend class here to enable
  // SetFlags for feature controlling need in test environment.
  friend class MediumEnvironment;
  void SetFlags(const Flags& flags) ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock(&mutex_);
    flags_ = flags;
  }
  Flags flags_ ABSL_GUARDED_BY(mutex_);
  mutable absl::Mutex mutex_;
};
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_BASE_FEATURE_FLAGS_H_
