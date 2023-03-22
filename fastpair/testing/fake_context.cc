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

#include "fastpair/testing/fake_context.h"

#include <memory>
#include "internal/test/fake_clock.h"
#include "internal/test/fake_device_info.h"
#include "internal/test/fake_timer.h"
#include "internal/test/fake_task_runner.h"

namespace nearby {
namespace fastpair {

FakeContext::FakeContext()
    : clock_(std::make_unique<FakeClock>()),
      device_info_(std::make_unique<FakeDeviceInfo>()) {}

Clock* FakeContext::GetClock() const { return clock_.get(); }

std::unique_ptr<Timer> FakeContext::CreateTimer() {
  FakeClock* fake_clock = dynamic_cast<FakeClock*>(clock_.get());
  return std::make_unique<FakeTimer>(fake_clock);
}

DeviceInfo* FakeContext::GetDeviceInfo() const { return device_info_.get(); }

std::unique_ptr<TaskRunner> FakeContext::CreateSequencedTaskRunner() const {
  std::unique_ptr<TaskRunner> task_runner =
      std::make_unique<FakeTaskRunner>(dynamic_cast<FakeClock*>(GetClock()), 1);
  return task_runner;
}

std::unique_ptr<TaskRunner> FakeContext::CreateConcurrentTaskRunner(
    uint32_t concurrent_count) const {
  std::unique_ptr<TaskRunner> task_runner = std::make_unique<FakeTaskRunner>(
      dynamic_cast<FakeClock*>(GetClock()), concurrent_count);
  return task_runner;
}

}  // namespace fastpair
}  // namespace nearby
