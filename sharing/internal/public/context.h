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

#ifndef THIRD_PARTY_NEARBY_SHARING_INTERNAL_PUBLIC_CONTEXT_H_
#define THIRD_PARTY_NEARBY_SHARING_INTERNAL_PUBLIC_CONTEXT_H_

#include <stdint.h>

#include <functional>
#include <memory>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "internal/network/url.h"
#include "internal/platform/clock.h"
#include "internal/platform/task_runner.h"
#include "internal/platform/timer.h"
#include "sharing/internal/api/bluetooth_adapter.h"
#include "sharing/internal/api/fast_initiation_manager.h"
#include "sharing/internal/api/shell.h"
#include "sharing/internal/api/wifi_adapter.h"
#include "sharing/internal/public/connectivity_manager.h"

namespace nearby {

// Context defines platform implementation-related interfaces. Nearby Sharing
// components should access these interfaces under their environment.
// On different platforms, the implementation of these interfaces are different.
// In order to support test cases on Google3, Nearby Sharing SDK also provides
// a mock implementation of these interfaces.
class Context {
 public:
  virtual ~Context() = default;

  virtual Clock* GetClock() const = 0;
  virtual std::unique_ptr<Timer> CreateTimer() = 0;

  // Opens a URL by calling the platform API. The platform API should run
  // in async mode. However, the callback might be called before
  // this function returns, for example if the URL has an error.
  // |url| is the URL to open. |callback| is called when the platform API
  // completes. absl::StatusCode::kOk is returned when the URL opens
  // successfully.
  virtual void OpenUrl(const nearby::network::Url& url,
                       std::function<void(absl::Status)> callback) = 0;
  virtual ConnectivityManager* GetConnectivityManager() const = 0;
  virtual sharing::api::BluetoothAdapter& GetBluetoothAdapter() const = 0;
  virtual sharing::api::WifiAdapter& GetWifiAdapter() const = 0;
  virtual api::FastInitiationManager& GetFastInitiationManager() const = 0;
  virtual std::unique_ptr<TaskRunner> CreateSequencedTaskRunner() const = 0;
  virtual void CopyText(absl::string_view text,
                        std::function<void(absl::Status)> callback) = 0;

  // Creates task runner concurrently. |concurrent_count| is the maximum
  // count of tasks running at the same time.
  virtual std::unique_ptr<TaskRunner> CreateConcurrentTaskRunner(
      uint32_t concurrent_count) const = 0;
  virtual api::Shell& GetShell() const = 0;

  // Provides the API to retrieve TaskRunner to run a task globally.
  virtual TaskRunner* GetTaskRunner() = 0;
};

}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_PUBLIC_CONTEXT_H_
