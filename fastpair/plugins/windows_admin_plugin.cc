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

#include "fastpair/plugins/windows_admin_plugin.h"

#include "fastpair/internal/fast_pair_seeker_impl.h"

namespace nearby {
namespace fastpair {

void WindowsAdminPlugin::PluginState::DiscoveryClicked(DiscoveryAction action) {
  NEARBY_LOGS(INFO) << __func__;
  if (device == nullptr || fast_pair_service == nullptr) return;
  absl::Status status = fast_pair_service->GetSeeker()->StartInitialPairing(
      *device, InitialPairingParam{},
      {.on_pairing_result = [](const FastPairDevice& device,
                               absl::Status status) {
        NEARBY_LOGS(INFO) << "Pairing result: " << status;
      }});
  NEARBY_LOGS(INFO) << "StartInitialPairing: " << status;
}

void WindowsAdminPlugin::PluginState::SetIsScreenLocked(bool is_locked) {
  NEARBY_LOGS(INFO) << __func__;
  if (device == nullptr || fast_pair_service == nullptr) return;
  FastPairSeekerExt* seeker =
      static_cast<FastPairSeekerExt*>(fast_pair_service->GetSeeker());
  seeker->SetIsScreenLocked(is_locked);
}

void WindowsAdminPlugin::OnInitialDiscoveryEvent(
    const InitialDiscoveryEvent& event) {
  NEARBY_LOGS(INFO) << "Received on initial discovery event";
  auto metadata = device_->GetMetadata();
  if (!metadata) {
    NEARBY_LOGS(INFO)
        << "Ignoring initial discovery event because metadata is missing";
    return;
  }
  state_->device = device_;
  for (auto* observer : state_->observers.GetObservers()) {
    observer->OnUpdateDevice(*metadata);
  }
}

void WindowsAdminPlugin::OnPairEvent(const PairEvent& event) {
  NEARBY_LOGS(INFO) << "Received on pair event";
  absl::Status status = seeker_->StartRetroactivePairing(
      *device_, RetroactivePairingParam{},
      {.on_pairing_result = [](const FastPairDevice& device,
                               absl::Status status) {
        NEARBY_LOGS(INFO) << "Pairing result: " << status;
        // TODO(jsobczak): Ask for user constent and save the Account Key to
        // user's account.
      }});
  NEARBY_LOGS(INFO) << "StartRetroactivePairing: " << status;
}

}  // namespace fastpair
}  // namespace nearby
