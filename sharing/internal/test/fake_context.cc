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

#include <functional>
#include <memory>
#include <utility>

#include "internal/platform/clock.h"
#include "internal/platform/task_runner.h"
#include "internal/platform/timer.h"
#include "internal/test/fake_clock.h"
#include "internal/test/fake_task_runner.h"
#include "internal/test/fake_timer.h"
#include "sharing/internal/api/bluetooth_adapter.h"
#include "sharing/internal/api/fast_initiation_manager.h"
#include "sharing/internal/api/wifi_adapter.h"
#include "sharing/internal/public/connectivity_manager.h"
#include "sharing/internal/test/fake_bluetooth_adapter.h"
#include "sharing/internal/test/fake_connectivity_manager.h"
#include "sharing/internal/test/fake_fast_initiation_manager.h"
#include "sharing/internal/test/fake_wifi_adapter.h"

namespace nearby {

FakeContext::FakeContext()
    : fake_clock_(std::make_unique<FakeClock>()),
      fake_connectivity_manager_(std::make_unique<FakeConnectivityManager>()),
      fake_bluetooth_adapter_(std::make_unique<FakeBluetoothAdapter>()),
      fake_wifi_adapter_(std::make_unique<FakeWifiAdapter>()),
      fake_fast_initiation_manager_(
          std::make_unique<FakeFastInitiationManager>()),
      executor_(std::make_unique<FakeTaskRunner>(fake_clock_.get(), 5)) {}

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

sharing::api::WifiAdapter& FakeContext::GetWifiAdapter() const {
  return *fake_wifi_adapter_;
}

api::FastInitiationManager& FakeContext::GetFastInitiationManager() const {
  return *fake_fast_initiation_manager_;
}

std::unique_ptr<TaskRunner> FakeContext::CreateSequencedTaskRunner() const {
  std::unique_ptr<TaskRunner> task_runner =
      std::make_unique<FakeTaskRunner>(fake_clock_.get(), 1);
  return task_runner;
}

std::unique_ptr<TaskRunner> FakeContext::CreateConcurrentTaskRunner(
    uint32_t concurrent_count) const {
  std::unique_ptr<TaskRunner> task_runner =
      std::make_unique<FakeTaskRunner>(fake_clock_.get(), concurrent_count);
  return task_runner;
}

TaskRunner* FakeContext::GetTaskRunner() { return executor_.get(); }

}  // namespace nearby
