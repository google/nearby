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

#include "fastpair/common/fast_pair_context_impl.h"

#include <memory>

#include "fastpair/internal/api/fast_pair_platform.h"
#include "internal/auth/authentication_manager_impl.h"
#include "internal/platform/device_info_impl.h"
#include "internal/platform/task_runner_impl.h"

namespace nearby {
namespace fastpair {

FastPairContextImpl::FastPairContextImpl()
    : preferences_manager_(std::make_unique<preferences::PreferencesManager>(
          nearby::fastpair::api::kFastPairPreferencesFilePath)),
      authentication_manager_(
          std::make_unique<auth::AuthenticationManagerImpl>()),
      device_info_(std::make_unique<DeviceInfoImpl>()),
      executor_(std::make_unique<TaskRunnerImpl>(5)) {}

preferences::PreferencesManager* FastPairContextImpl::GetPreferencesManager()
    const {
  return preferences_manager_.get();
}

auth::AuthenticationManager* FastPairContextImpl::GetAuthenticationManager()
    const {
  return authentication_manager_.get();
}

DeviceInfo* FastPairContextImpl::GetDeviceInfo() const {
  return device_info_.get();
}

TaskRunner* FastPairContextImpl::GetTaskRunner() { return executor_.get(); }

}  // namespace fastpair
}  // namespace nearby
