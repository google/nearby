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

#ifndef THIRD_PARTY_NEARBY_SHARING_INTERNAL_PUBLIC_CONTEXT_IMPL_H_
#define THIRD_PARTY_NEARBY_SHARING_INTERNAL_PUBLIC_CONTEXT_IMPL_H_

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
#include "sharing/internal/api/sharing_platform.h"
#include "sharing/internal/api/wifi_adapter.h"
#include "sharing/internal/public/connectivity_manager.h"
#include "sharing/internal/public/context.h"

namespace nearby {

class ContextImpl : public Context {
 public:
  explicit ContextImpl(nearby::sharing::api::SharingPlatform& platform);
  ~ContextImpl() override = default;

  Clock* GetClock() const override;
  std::unique_ptr<Timer> CreateTimer() override;
  ConnectivityManager* GetConnectivityManager() const override;
  sharing::api::BluetoothAdapter& GetBluetoothAdapter() const override;
  sharing::api::WifiAdapter& GetWifiAdapter() const override;
  api::FastInitiationManager& GetFastInitiationManager() const override;
  std::unique_ptr<TaskRunner> CreateSequencedTaskRunner() const override;
  std::unique_ptr<TaskRunner> CreateConcurrentTaskRunner(
      uint32_t concurrent_count) const override;
  TaskRunner* GetTaskRunner() override;

 private:
  nearby::sharing::api::SharingPlatform& platform_;
  std::unique_ptr<Clock> clock_;
  std::unique_ptr<ConnectivityManager> connectivity_manager_;
};

}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_PUBLIC_CONTEXT_IMPL_H_
