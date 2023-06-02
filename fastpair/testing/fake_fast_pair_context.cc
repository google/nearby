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

#include "fastpair/testing/fake_fast_pair_context.h"

#include <memory>

#include "fastpair/internal/api/fast_pair_platform.h"
#include "internal/test/fake_clock.h"
#include "internal/test/fake_device_info.h"
#include "internal/test/fake_task_runner.h"
#include "internal/test/google3_only/fake_authentication_manager.h"

namespace nearby {
namespace fastpair {

FakeFastPairContext::FakeFastPairContext()
    : clock_(std::make_unique<FakeClock>()),
      preferences_manager_(std::make_unique<preferences::PreferencesManager>(
          nearby::fastpair::api::kFastPairPreferencesFilePath)),
      authentication_manager_(std::make_unique<FakeAuthenticationManager>()),
      device_info_(std::make_unique<FakeDeviceInfo>()),
      executor_(std::make_unique<FakeTaskRunner>(
          dynamic_cast<FakeClock*>(clock_.get()), 5)) {}

preferences::PreferencesManager* FakeFastPairContext::GetPreferencesManager()
    const {
  return preferences_manager_.get();
}

auth::AuthenticationManager* FakeFastPairContext::GetAuthenticationManager()
    const {
  return authentication_manager_.get();
}

DeviceInfo* FakeFastPairContext::GetDeviceInfo() const {
  return device_info_.get();
}

TaskRunner* FakeFastPairContext::GetTaskRunner() { return executor_.get(); }

}  // namespace fastpair
}  // namespace nearby
