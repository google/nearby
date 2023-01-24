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

#ifndef PLATFORM_API_SCHEDULED_EXECUTOR_H_
#define PLATFORM_API_SCHEDULED_EXECUTOR_H_

#include <cstdint>
#include <memory>

#include "absl/time/time.h"
#include "internal/platform/implementation/cancelable.h"
#include "internal/platform/implementation/executor.h"
#include "internal/platform/runnable.h"

namespace nearby {
namespace api {

// An Executor that can schedule commands to run after a given delay, or to
// execute periodically.
//
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/ScheduledExecutorService.html
class ScheduledExecutor : public Executor {
 public:
  ~ScheduledExecutor() override = default;
  // Cancelable is kept both in the executor context, and in the caller context.
  // We want Cancelable to live until both caller and executor are done with it.
  // Exclusive ownership model does not work for this case;
  // using std:shared_ptr<> instead if std::unique_ptr<>.
  virtual std::shared_ptr<Cancelable> Schedule(Runnable&& runnable,
                                               absl::Duration duration) = 0;
};

}  // namespace api
}  // namespace nearby

#endif  // PLATFORM_API_SCHEDULED_EXECUTOR_H_
