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

#include "fastpair/plugins/fake_fast_pair_plugin.h"

namespace nearby {
namespace fastpair {

void FakeFastPairPlugin::OnInitialDiscoveryEvent(
    const InitialDiscoveryEvent& event) {
  if (provider_->on_initial_discovery_event_ != nullptr) {
    provider_->on_initial_discovery_event_(device_, event);
  }
}

void FakeFastPairPlugin::OnScreenEvent(const ScreenEvent& event) {
  if (provider_->on_screen_event_ != nullptr) {
    provider_->on_screen_event_(event);
  }
}

}  // namespace fastpair
}  // namespace nearby
