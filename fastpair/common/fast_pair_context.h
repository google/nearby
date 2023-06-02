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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_COMMON_FAST_PAIR_CONTEXT_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_COMMON_FAST_PAIR_CONTEXT_H_

#include "internal/auth/authentication_manager.h"
#include "internal/platform/device_info.h"
#include "internal/platform/task_runner.h"
#include "internal/preferences/preferences_manager.h"

namespace nearby {
namespace fastpair {

// Provides an interface which can be overridden to
// provide mock or fake implementations.
class FastPairContext {
 public:
  virtual ~FastPairContext() = default;

  virtual preferences::PreferencesManager* GetPreferencesManager() const = 0;
  virtual auth::AuthenticationManager* GetAuthenticationManager() const = 0;
  virtual DeviceInfo* GetDeviceInfo() const = 0;

  // Provides the API to retrieve TaskRunner to run a task globally.
  virtual TaskRunner* GetTaskRunner() = 0;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_COMMON_FAST_PAIR_CONTEXT_H_
