// Copyright 2021-2023 Google LLC
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

#include "sharing/internal/test/fake_context.h"

#include <stdint.h>

#include <memory>

#include "internal/platform/clock.h"
#include "internal/platform/single_thread_executor.h"
#include "internal/platform/task_runner.h"
#include "internal/platform/timer.h"
#include "internal/test/fake_clock.h"
#include "internal/test/fake_task_runner.h"
#include "internal/test/fake_timer.h"
#include "internal/test/test_single_thread_executor.h"
#include "sharing/internal/api/bluetooth_adapter.h"
#include "sharing/internal/api/fast_initiation_manager.h"
#include "sharing/internal/public/connectivity_manager.h"
#include "sharing/internal/test/fake_bluetooth_adapter.h"
#include "sharing/internal/test/fake_connectivity_manager.h"
#include "sharing/internal/test/fake_fast_initiation_manager.h"

namespace nearby {

FakeContext::FakeContext()
    : fake_clock_(std::make_unique<FakeClock>()),
      fake_connectivity_manager_(std::make_unique<FakeConnectivityManager>()),
      fake_bluetooth_adapter_(std::make_unique<FakeBluetoothAdapter>()),
      fake_fast_initiation_manager_(
          std::make_unique<FakeFastInitiationManager>()) {}

Clock* FakeContext::GetClock() const { return fake_clock_.get(); }

std::unique_ptr<Timer> FakeContext::CreateTimer() {
  return std::make_unique<FakeTimer>(fake_clock_.get());
}

ConnectivityManager* FakeContext::GetConnectivityManager() const {
  return fake_connectivity_manager_.get();
}

sharing::api::BluetoothAdapter& FakeContext::GetBluetoothAdapter() const {
  return *fake_bluetooth_adapter_;
}

api::FastInitiationManager& FakeContext::GetFastInitiationManager() const {
  return *fake_fast_initiation_manager_;
}

std::unique_ptr<SingleThreadExecutor> FakeContext::CreateSingleThreadExecutor()
    const {
  auto executor = std::make_unique<TestSingleThreadExecutor>(fake_clock_.get());
  last_single_thread_executor_ = executor.get();
  return executor;
}

std::unique_ptr<TaskRunner> FakeContext::CreateConcurrentTaskRunner(
    uint32_t concurrent_count) const {
  return std::make_unique<FakeTaskRunner>(fake_clock_.get(), concurrent_count);
}

}  // namespace nearby
