// Copyright 2022 Google LLC
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

#include "sharing/fake_nearby_sharing_service.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "internal/base/observer_list.h"
#include "sharing/attachment.h"
#include "sharing/internal/api/sharing_rpc_notifier.h"
#include "sharing/local_device_data/nearby_share_local_device_data_manager.h"
#include "sharing/nearby_sharing_service.h"
#include "sharing/nearby_sharing_settings.h"
#include "sharing/share_target.h"
#include "sharing/share_target_discovered_callback.h"
#include "sharing/transfer_metadata.h"
#include "sharing/transfer_update_callback.h"

namespace nearby {
namespace sharing {

using ::nearby::sharing::api::SharingRpcNotifier;

void FakeNearbySharingService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeNearbySharingService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}
bool FakeNearbySharingService::HasObserver(Observer* observer) {
  return observers_.HasObserver(observer);
}

// Shutdown the Nearby Sharing service, and cleanup.
void FakeNearbySharingService::Shutdown(
    std::function<void(StatusCodes)> status_codes_callback) {
  status_codes_callback(StatusCodes::kOk);
}

// Registers a send surface for handling payload transfer status and device
// discovery.
void FakeNearbySharingService::RegisterSendSurface(
    TransferUpdateCallback* transfer_callback,
    ShareTargetDiscoveredCallback* discovery_callback, SendSurfaceState state,
    std::function<void(StatusCodes)> status_codes_callback) {
  if (state == SendSurfaceState::kForeground) {
    foreground_send_transfer_callbacks_.AddObserver(transfer_callback);
    foreground_send_discovered_callbacks_.AddObserver(discovery_callback);
  } else {
    background_send_transfer_callbacks_.AddObserver(transfer_callback);
    background_send_discovered_callbacks_.AddObserver(discovery_callback);
  }

  status_codes_callback(StatusCodes::kOk);
}

// Unregisters the current send surface.
void FakeNearbySharingService::UnregisterSendSurface(
    TransferUpdateCallback* transfer_callback,
    ShareTargetDiscoveredCallback* discovery_callback,
    std::function<void(StatusCodes)> status_codes_callback) {
  foreground_send_transfer_callbacks_.RemoveObserver(transfer_callback);
  foreground_send_discovered_callbacks_.RemoveObserver(discovery_callback);
  background_send_transfer_callbacks_.RemoveObserver(transfer_callback);
  background_send_discovered_callbacks_.RemoveObserver(discovery_callback);

  status_codes_callback(StatusCodes::kOk);
}

// Registers a receiver surface for handling payload transfer status.
void FakeNearbySharingService::RegisterReceiveSurface(
    TransferUpdateCallback* transfer_callback, ReceiveSurfaceState state,
    std::function<void(StatusCodes)> status_codes_callback) {
  if (state == ReceiveSurfaceState::kForeground) {
    foreground_receive_transfer_callbacks_.AddObserver(transfer_callback);
  } else {
    background_receive_transfer_callbacks_.AddObserver(transfer_callback);
  }

  status_codes_callback(StatusCodes::kOk);
}

// Unregisters the current receive surface.
void FakeNearbySharingService::UnregisterReceiveSurface(
    TransferUpdateCallback* transfer_callback,
    std::function<void(StatusCodes)> status_codes_callback) {
  foreground_receive_transfer_callbacks_.RemoveObserver(transfer_callback);
  background_receive_transfer_callbacks_.RemoveObserver(transfer_callback);
  status_codes_callback(StatusCodes::kOk);
}

// Unregisters all foreground receive surfaces.
void FakeNearbySharingService::ClearForegroundReceiveSurfaces(
    std::function<void(StatusCodes)> status_codes_callback) {
  status_codes_callback(StatusCodes::kOk);
}

// Returns true if a foreground receive surface is registered.
bool FakeNearbySharingService::IsInHighVisibility() const { return false; }

// Returns true if there is an ongoing file transfer.
bool FakeNearbySharingService::IsTransferring() const { return false; }

// Returns true if we're currently receiving a file.
bool FakeNearbySharingService::IsReceivingFile() const { return false; }

// Returns true if we're currently sending a file.
bool FakeNearbySharingService::IsSendingFile() const { return false; }

// Returns true if we're currently attempting to connect to a
// remote device.
bool FakeNearbySharingService::IsConnecting() const { return false; }

// Returns true if we are currently scanning for remote devices.
bool FakeNearbySharingService::IsScanning() const { return false; }

// Sends |attachments| to the remote |share_target|.
void FakeNearbySharingService::SendAttachments(
    const ShareTarget& share_target,
    std::vector<std::unique_ptr<Attachment>> attachments,
    std::function<void(StatusCodes)> status_codes_callback) {
  status_codes_callback(StatusCodes::kOk);
}

// Accepts incoming share from the remote |share_target|.
void FakeNearbySharingService::Accept(
    const ShareTarget& share_target,
    std::function<void(StatusCodes status_codes)> status_codes_callback) {
  status_codes_callback(StatusCodes::kOk);
}

// Rejects incoming share from the remote |share_target|.
void FakeNearbySharingService::Reject(
    const ShareTarget& share_target,
    std::function<void(StatusCodes status_codes)> status_codes_callback) {
  status_codes_callback(StatusCodes::kOk);
}

// Cancels outgoing shares to the remote |share_target|.
void FakeNearbySharingService::Cancel(
    const ShareTarget& share_target,
    std::function<void(StatusCodes status_codes)> status_codes_callback) {
  status_codes_callback(StatusCodes::kOk);
}

// Returns true if the local user cancelled the transfer to remote
// |share_target|.
bool FakeNearbySharingService::DidLocalUserCancelTransfer(
    const ShareTarget& share_target) {
  return false;
}

// Opens attachments from the remote |share_target|.
void FakeNearbySharingService::Open(
    const ShareTarget& share_target,
    std::function<void(StatusCodes status_codes)> status_codes_callback) {
  status_codes_callback(StatusCodes::kOk);
}

// Opens an url target on a browser instance.
void FakeNearbySharingService::OpenUrl(const ::nearby::network::Url& url) {}

// Copies text to cache/clipboard.
void FakeNearbySharingService::CopyText(absl::string_view text) {}

// Sets a cleanup callback to be called once done with transfer for ARC.
void FakeNearbySharingService::SetArcTransferCleanupCallback(
    std::function<void()> callback) {}

std::string FakeNearbySharingService::Dump() const { return ""; }

NearbyShareSettings* FakeNearbySharingService::GetSettings() { return nullptr; }

SharingRpcNotifier* FakeNearbySharingService::GetRpcNotifier() {
  return nullptr;
}

NearbyShareLocalDeviceDataManager*
FakeNearbySharingService::GetLocalDeviceDataManager() {
  return nullptr;
}

NearbyShareContactManager* FakeNearbySharingService::GetContactManager() {
  return nullptr;
}

NearbyShareCertificateManager*
FakeNearbySharingService::GetCertificateManager() {
  return nullptr;
}

AccountManager* FakeNearbySharingService::GetAccountManager() {
  return nullptr;
}

void FakeNearbySharingService::FireHighVisibilityChangeRequested() {
  for (auto& observer : observers_.GetObservers()) {
    observer->OnHighVisibilityChangeRequested();
  }
}

void FakeNearbySharingService::FireHighVisibilityChanged(
    bool in_high_visibility) {
  for (auto& observer : observers_.GetObservers()) {
    observer->OnHighVisibilityChanged(in_high_visibility);
  }
}

void FakeNearbySharingService::FireStartAdvertisingFailure() {
  for (auto& observer : observers_.GetObservers()) {
    observer->OnStartAdvertisingFailure();
  }
}

void FakeNearbySharingService::FireStartDiscoveryResult(bool success) {
  for (auto& observer : observers_.GetObservers()) {
    observer->OnStartDiscoveryResult(success);
  }
}

void FakeNearbySharingService::FireFastInitiationDevicesDetected() {
  for (auto& observer : observers_.GetObservers()) {
    observer->OnFastInitiationDevicesDetected();
  }
}

void FakeNearbySharingService::FireFastInitiationDevicesNotDetected() {
  for (auto& observer : observers_.GetObservers()) {
    observer->OnFastInitiationDevicesNotDetected();
  }
}

void FakeNearbySharingService::FireFastInitiationScanningStopped() {
  for (auto& observer : observers_.GetObservers()) {
    observer->OnFastInitiationScanningStopped();
  }
}

void FakeNearbySharingService::FireShutdown() {
  for (auto& observer : observers_.GetObservers()) {
    observer->OnShutdown();
  }
}

void FakeNearbySharingService::FireSendTransferUpdate(
    SendSurfaceState state, ShareTarget share_target,
    TransferMetadata transfer_metadata) {
  if (state == SendSurfaceState::kForeground) {
    for (auto& transfer_callback :
         foreground_send_transfer_callbacks_.GetObservers()) {
      transfer_callback->OnTransferUpdate(share_target, transfer_metadata);
    }
  } else {
    for (auto& transfer_callback :
         background_send_transfer_callbacks_.GetObservers()) {
      transfer_callback->OnTransferUpdate(share_target, transfer_metadata);
    }
  }
}

void FakeNearbySharingService::FireReceiveTransferUpdate(
    ReceiveSurfaceState state, ShareTarget share_target,
    TransferMetadata transfer_metadata) {
  if (state == ReceiveSurfaceState::kForeground) {
    for (auto& transfer_callback :
         foreground_receive_transfer_callbacks_.GetObservers()) {
      transfer_callback->OnTransferUpdate(share_target, transfer_metadata);
    }
  } else {
    for (auto& transfer_callback :
         foreground_receive_transfer_callbacks_.GetObservers()) {
      transfer_callback->OnTransferUpdate(share_target, transfer_metadata);
    }
  }
}

// Fire discovery events.
void FakeNearbySharingService::FireShareTargetDiscovered(
    SendSurfaceState state, ShareTarget share_target) {
  if (state == SendSurfaceState::kForeground) {
    for (auto& discovered_callback :
         foreground_send_discovered_callbacks_.GetObservers()) {
      discovered_callback->OnShareTargetDiscovered(share_target);
    }
  } else {
    for (auto& discovered_callback :
         background_send_discovered_callbacks_.GetObservers()) {
      discovered_callback->OnShareTargetDiscovered(share_target);
    }
  }
}

void FakeNearbySharingService::FireShareTargetLost(SendSurfaceState state,
                                                   ShareTarget share_target) {
  if (state == SendSurfaceState::kForeground) {
    for (auto& discovered_callback :
         foreground_send_discovered_callbacks_.GetObservers()) {
      discovered_callback->OnShareTargetLost(share_target);
    }
  } else {
    for (auto& discovered_callback :
         background_send_discovered_callbacks_.GetObservers()) {
      discovered_callback->OnShareTargetLost(share_target);
    }
  }
}

}  // namespace sharing
}  // namespace nearby
