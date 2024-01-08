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

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "internal/network/url.h"
#include "internal/platform/clock.h"
#include "internal/platform/task_runner.h"
#include "internal/platform/timer.h"
#include "internal/test/fake_clock.h"
#include "internal/test/fake_task_runner.h"
#include "internal/test/fake_timer.h"
#include "sharing/internal/api/bluetooth_adapter.h"
#include "sharing/internal/api/fast_initiation_manager.h"
#include "sharing/internal/api/shell.h"
#include "sharing/internal/api/wifi_adapter.h"
#include "sharing/internal/public/connectivity_manager.h"
#include "sharing/internal/test/fake_bluetooth_adapter.h"
#include "sharing/internal/test/fake_connectivity_manager.h"
#include "sharing/internal/test/fake_fast_initiation_manager.h"
#include "sharing/internal/test/fake_shell.h"
#include "sharing/internal/test/fake_wifi_adapter.h"

namespace nearby {

FakeContext::FakeContext()
    : fake_clock_(std::make_unique<FakeClock>()),
      connectivity_manager_(std::make_unique<FakeConnectivityManager>()),
      bluetooth_adapter_(std::make_unique<FakeBluetoothAdapter>()),
      wifi_adapter_(std::make_unique<FakeWifiAdapter>()),
      fast_initiation_manager_(std::make_unique<FakeFastInitiationManager>()),
      shell_(std::make_unique<FakeShell>()),
      executor_(std::make_unique<FakeTaskRunner>(
          dynamic_cast<FakeClock*>(GetClock()), 5)) {}

Clock* FakeContext::GetClock() const { return fake_clock_.get(); }

std::unique_ptr<Timer> FakeContext::CreateTimer() {
  return std::make_unique<FakeTimer>(fake_clock_.get());
}

void FakeContext::OpenUrl(const nearby::network::Url& url,
                          std::function<void(absl::Status)> callback) {
  // OpenUrl is an interface that depends on platform API. In a mock method, it
  // returns OK to avoid breaking test cases in the Nearby Sharing SDK.
  std::move(callback)(absl::OkStatus());
}

void FakeContext::CopyText(const absl::string_view text,
                           std::function<void(absl::Status)> callback) {
  // CopyText is an interface that depends on platform API. In a mock method, it
  // returns OK to avoid breaking test cases in the Nearby Sharing SDK.
  std::move(callback)(absl::OkStatus());
}

ConnectivityManager* FakeContext::GetConnectivityManager() const {
  return connectivity_manager_.get();
}

sharing::api::BluetoothAdapter& FakeContext::GetBluetoothAdapter() const {
  return *bluetooth_adapter_;
}

sharing::api::WifiAdapter& FakeContext::GetWifiAdapter() const {
  return *wifi_adapter_;
}

api::FastInitiationManager& FakeContext::GetFastInitiationManager() const {
  return *fast_initiation_manager_;
}

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

api::Shell& FakeContext::GetShell() const { return *shell_; }

TaskRunner* FakeContext::GetTaskRunner() { return executor_.get(); }

}  // namespace nearby
