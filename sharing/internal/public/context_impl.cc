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

#include "sharing/internal/public/context_impl.h"

#include <stdint.h>

#include <functional>
#include <memory>
#include <utility>

#include "internal/platform/clock.h"
#include "internal/platform/clock_impl.h"
#include "internal/platform/task_runner.h"
#include "internal/platform/task_runner_impl.h"
#include "internal/platform/timer.h"
#include "internal/platform/timer_impl.h"
#include "sharing/internal/api/bluetooth_adapter.h"
#include "sharing/internal/api/fast_initiation_manager.h"
#include "sharing/internal/api/sharing_platform.h"
#include "sharing/internal/api/wifi_adapter.h"
#include "sharing/internal/public/connectivity_manager.h"
#include "sharing/internal/public/connectivity_manager_impl.h"

namespace nearby {

using ::nearby::sharing::api::SharingPlatform;

ContextImpl::ContextImpl(SharingPlatform& platform)
    : platform_(platform),
      clock_(std::make_unique<ClockImpl>()),
      connectivity_manager_(
          std::make_unique<ConnectivityManagerImpl>(platform_)) {}

Clock* ContextImpl::GetClock() const { return clock_.get(); }

std::unique_ptr<Timer> ContextImpl::CreateTimer() {
  return std::make_unique<TimerImpl>();
}

ConnectivityManager* ContextImpl::GetConnectivityManager() const {
  return connectivity_manager_.get();
}

sharing::api::BluetoothAdapter& ContextImpl::GetBluetoothAdapter() const {
  return platform_.GetBluetoothAdapter();
}

sharing::api::WifiAdapter& ContextImpl::GetWifiAdapter() const {
  return platform_.GetWifiAdapter();
}

api::FastInitiationManager& ContextImpl::GetFastInitiationManager() const {
  return platform_.GetFastInitiationManager();
}

std::unique_ptr<TaskRunner> ContextImpl::CreateSequencedTaskRunner() const {
  std::unique_ptr<TaskRunner> task_runner = std::make_unique<TaskRunnerImpl>(1);
  return task_runner;
}

std::unique_ptr<TaskRunner> ContextImpl::CreateConcurrentTaskRunner(
    uint32_t concurrent_count) const {
  std::unique_ptr<TaskRunner> task_runner =
      std::make_unique<TaskRunnerImpl>(concurrent_count);
  return task_runner;
}

TaskRunner* ContextImpl::GetTaskRunner() {
  return &platform_.GetDefaultTaskRunner();
}

}  // namespace nearby
