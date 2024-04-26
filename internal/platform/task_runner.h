// Copyright 2022 Google LLC
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

#ifndef PLATFORM_PUBLIC_TASK_RUNNER_H_
#define PLATFORM_PUBLIC_TASK_RUNNER_H_

#include "absl/functional/any_invocable.h"
#include "absl/time/time.h"

namespace nearby {

// Task runner is an implementation to run tasks immediately or with a delay.
// The current implementation does not allow running nested tasks.
class TaskRunner {
 public:
  virtual ~TaskRunner() = default;

  // Posts a task to task runner. The task runs immediately or not depends on
  // the implementation of class. If the implementation supports multiple
  // threads, posted tasks could run concurrently.
  // Returns false if TashRunner has been shutdown.
  virtual bool PostTask(absl::AnyInvocable<void()> task) = 0;

  // Posts a task to run with delay. Multiple tasks can be scheduled. Tasks will
  // execute in the order of their delay expiring, not in the order they were
  // posted.
  // Returns false if TashRunner has been shutdown.
  virtual bool PostDelayedTask(absl::Duration delay,
                               absl::AnyInvocable<void()> task) = 0;

  // Shutdown this TaskRunner so that scheduled tasks are no longer executed.
  // New tasks posted after Shutdown all will be ignored.
  virtual void Shutdown() = 0;
};

}  // namespace nearby

#endif  // PLATFORM_PUBLIC_TASK_RUNNER_H_
