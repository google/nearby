// Copyright 2023 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_COMMON_CONTEXT_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_COMMON_CONTEXT_H_

#include <memory>

#include "internal/platform/clock.h"
#include "internal/platform/device_info.h"
#include "internal/platform/timer.h"
#include "internal/platform/task_runner.h"

namespace nearby {
namespace fastpair {

// Provides a mock implementation of interfaces to
// support test cases on Google3.
class Context {
 public:
  virtual ~Context() = default;

  virtual Clock* GetClock() const = 0;
  virtual std::unique_ptr<Timer> CreateTimer() = 0;
  virtual DeviceInfo* GetDeviceInfo() const = 0;
  virtual std::unique_ptr<TaskRunner> CreateSequencedTaskRunner() const = 0;
  // Creates task runner concurrently. |concurrent_count| is the maximum
  // count of tasks running at the same time.
  virtual std::unique_ptr<TaskRunner> CreateConcurrentTaskRunner(
      uint32_t concurrent_count) const = 0;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_COMMON_CONTEXT_H_
