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
  switch (action) {
    case DiscoveryAction::kPairToDevice: {
      NEARBY_LOGS(INFO) << __func__ << ": Action =  kPairToDevice";
      absl::Status status = fast_pair_service->GetSeeker()->StartInitialPairing(
          *device, InitialPairingParam{},
          {.on_pairing_result = [this](const FastPairDevice& callback_device,
                                       absl::Status status) {
            NEARBY_LOGS(INFO) << "Show pairing result: " << status;
            for (auto* observer : observers.GetObservers()) {
              observer->OnPairingResult(
                  const_cast<FastPairDevice&>(callback_device), status.ok());
            }
          }});
      NEARBY_LOGS(INFO) << "StartInitialPairing: " << status;
    } break;
    case DiscoveryAction::kSaveDeviceToAccount: {
      NEARBY_LOGS(INFO) << __func__ << ": Action = kSaveDeviceToAccount";
      absl::Status status =
          fast_pair_service->GetSeeker()->FinishRetroactivePairing(
              *device, FinishRetroactivePairingParam{.save_account_key = true},
              {.on_pairing_result = [](const FastPairDevice& device,
                                       absl::Status status) {
                NEARBY_LOGS(INFO) << "Finish retro result: " << status;
              }});
      foreground_currently_showing_notification = false;
      NEARBY_LOGS(INFO) << "FinishRetroactivePairing: " << status;
    } break;
    case DiscoveryAction::kDismissedByOs:
      NEARBY_LOGS(INFO) << __func__ << ": Action =  kDismissedByOs";
      break;
    case DiscoveryAction::kDismissedByUser:
      // When the user explicitly dismisses the discovery notification, update
      // the device's block-list value accordingly.
      NEARBY_LOGS(INFO) << __func__ << ": Action =  kDismissedByUser";
      foreground_currently_showing_notification = false;
      break;
    case DiscoveryAction::kDismissedByTimeout:
      NEARBY_LOGS(INFO) << __func__ << ": Action =  kDismissedByTimeout";
      foreground_currently_showing_notification = false;
      break;
    case DiscoveryAction::kLearnMore:
      NEARBY_LOGS(INFO) << __func__ << ": Action =  kLearnMore";
      break;
    case DiscoveryAction::kDone:
      NEARBY_LOGS(INFO) << __func__ << ": Action =  kDone";
      foreground_currently_showing_notification = false;
      break;
    default:
      NEARBY_LOGS(INFO) << __func__ << ": Action =  Unknown";
      break;
  }
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
  if (device_->ShouldShowUiNotification().has_value() &&
      !device_->ShouldShowUiNotification().value()) {
    NEARBY_LOGS(INFO) << __func__ << ": Ignoring because show UI flag is false";
    return;
  }
  if (state_->foreground_currently_showing_notification) {
    NEARBY_LOGS(VERBOSE) << __func__
                         << ": Already showing a notification for a device";
    return;
  }
  NotifyShowNotification(*device_);
}

void WindowsAdminPlugin::OnPairEvent(const PairEvent& event) {
  NEARBY_LOGS(INFO) << "Received on pair event";
  absl::Status status = seeker_->StartRetroactivePairing(
      *device_, RetroactivePairingParam{},
      {.on_pairing_result = [this](const FastPairDevice& device,
                                   absl::Status status) {
        NEARBY_LOGS(INFO) << "Retroactive Pairing result: " << status;
        if (!status.ok()) return;
        NotifyShowNotification(device);
      }});
  NEARBY_LOGS(INFO) << "StartRetroactivePairing: " << status;
}

void WindowsAdminPlugin::NotifyShowNotification(const FastPairDevice& device) {
  NEARBY_LOGS(INFO) << __func__;
  state_->foreground_currently_showing_notification = true;
  state_->device = &device;
  for (auto* observer : state_->observers.GetObservers()) {
    observer->OnUpdateDevice(const_cast<FastPairDevice&>(device));
  }
}
}  // namespace fastpair
}  // namespace nearby
