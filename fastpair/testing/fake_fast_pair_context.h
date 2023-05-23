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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_TESTING_FAKE_FAST_PAIR_CONTEXT_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_TESTING_FAKE_FAST_PAIR_CONTEXT_H_

#include <memory>

#include "fastpair/common/fast_pair_context.h"
#include "internal/platform/clock.h"

namespace nearby {
namespace fastpair {
class FakeFastPairContext : public FastPairContext {
 public:
  FakeFastPairContext();
  ~FakeFastPairContext() = default;

  preferences::PreferencesManager* GetPreferencesManager() const override;
  auth::AuthenticationManager* GetAuthenticationManager() const override;
  DeviceInfo* GetDeviceInfo() const override;
  TaskRunner* GetTaskRunner() override;

 private:
  std::unique_ptr<Clock> clock_ = nullptr;
  std::unique_ptr<preferences::PreferencesManager> preferences_manager_ =
      nullptr;
  std::unique_ptr<auth::AuthenticationManager> authentication_manager_ =
      nullptr;
  std::unique_ptr<DeviceInfo> device_info_ = nullptr;
  std::unique_ptr<TaskRunner> executor_ = nullptr;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_TESTING_FAKE_FAST_PAIR_CONTEXT_H_
