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

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "absl/strings/string_view.h"
#include "internal/base/observer_list.h"
#include "sharing/advertisement.h"
#include "sharing/attachment_container.h"
#include "sharing/internal/api/sharing_rpc_notifier.h"
#include "sharing/local_device_data/nearby_share_local_device_data_manager.h"
#include "sharing/nearby_sharing_service.h"
#include "sharing/nearby_sharing_settings.h"
#include "sharing/share_target.h"
#include "sharing/share_target_discovered_callback.h"
#include "sharing/transfer_metadata.h"
#include "sharing/transfer_update_callback.h"
#include "sharing/wrapped_share_target_discovered_callback.h"

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
    foreground_send_surface_map_.insert(
        {transfer_callback,
         WrappedShareTargetDiscoveredCallback(
             discovery_callback, Advertisement::BlockedVendorId::kNone,
             /*disable_wifi_hotspot=*/false)});
  } else {
    background_send_surface_map_.insert(
        {transfer_callback,
         WrappedShareTargetDiscoveredCallback(
             discovery_callback, Advertisement::BlockedVendorId::kNone,
             /*disable_wifi_hotspot=*/false)});
  }

  status_codes_callback(StatusCodes::kOk);
}

// Unregisters the current send surface.
void FakeNearbySharingService::UnregisterSendSurface(
    TransferUpdateCallback* transfer_callback,
    std::function<void(StatusCodes)> status_codes_callback) {
  foreground_send_surface_map_.erase(transfer_callback);
  background_send_surface_map_.erase(transfer_callback);

  status_codes_callback(StatusCodes::kOk);
}

// Registers a receiver surface for handling payload transfer status.
void FakeNearbySharingService::RegisterReceiveSurface(
    TransferUpdateCallback* transfer_callback, ReceiveSurfaceState state,
    Advertisement::BlockedVendorId vendor_id,
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

// Returns true if there is an ongoing file transfer.
bool FakeNearbySharingService::IsTransferring() const { return false; }

// Returns true if we are currently scanning for remote devices.
bool FakeNearbySharingService::IsScanning() const { return false; }

// Sends |attachments| to the remote |share_target|.
void FakeNearbySharingService::SendAttachments(
    int64_t share_target_id,
    std::unique_ptr<AttachmentContainer> attachment_container,
    std::function<void(StatusCodes)> status_codes_callback) {
  status_codes_callback(StatusCodes::kOk);
}

// Accepts incoming share from the remote |share_target|.
void FakeNearbySharingService::Accept(
    int64_t share_target_id,
    std::function<void(StatusCodes status_codes)> status_codes_callback) {
  status_codes_callback(StatusCodes::kOk);
}

// Rejects incoming share from the remote |share_target|.
void FakeNearbySharingService::Reject(
    int64_t share_target_id,
    std::function<void(StatusCodes status_codes)> status_codes_callback) {
  status_codes_callback(StatusCodes::kOk);
}

// Cancels outgoing shares to the remote |share_target|.
void FakeNearbySharingService::Cancel(
    int64_t share_target_id,
    std::function<void(StatusCodes status_codes)> status_codes_callback) {
  status_codes_callback(StatusCodes::kOk);
}

// Returns true if the local user cancelled the transfer to remote
// |share_target|.
bool FakeNearbySharingService::DidLocalUserCancelTransfer(
    int64_t share_target_id) {
  return false;
}

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
    SendSurfaceState state, const ShareTarget& share_target,
    const AttachmentContainer& attachment_container,
    TransferMetadata transfer_metadata) {
  if (state == SendSurfaceState::kForeground) {
    for (auto& entry : foreground_send_surface_map_) {
      entry.first->OnTransferUpdate(share_target, attachment_container,
                                    transfer_metadata);
    }
  } else {
    for (auto& entry : background_send_surface_map_) {
      entry.first->OnTransferUpdate(share_target, attachment_container,
                                    transfer_metadata);
    }
  }
}

void FakeNearbySharingService::FireReceiveTransferUpdate(
    ReceiveSurfaceState state, const ShareTarget& share_target,
    const AttachmentContainer& attachment_container,
    TransferMetadata transfer_metadata) {
  if (state == ReceiveSurfaceState::kForeground) {
    for (auto& transfer_callback :
         foreground_receive_transfer_callbacks_.GetObservers()) {
      transfer_callback->OnTransferUpdate(share_target, attachment_container,
                                          transfer_metadata);
    }
  } else {
    for (auto& transfer_callback :
         foreground_receive_transfer_callbacks_.GetObservers()) {
      transfer_callback->OnTransferUpdate(share_target, attachment_container,
                                          transfer_metadata);
    }
  }
}

// Fire discovery events.
void FakeNearbySharingService::FireShareTargetDiscovered(
    SendSurfaceState state, ShareTarget share_target) {
  if (state == SendSurfaceState::kForeground) {
    for (auto& entry : foreground_send_surface_map_) {
      entry.second.OnShareTargetDiscovered(share_target);
    }
  } else {
    for (auto& entry : background_send_surface_map_) {
      entry.second.OnShareTargetDiscovered(share_target);
    }
  }
}

void FakeNearbySharingService::FireShareTargetLost(SendSurfaceState state,
                                                   ShareTarget share_target) {
  if (state == SendSurfaceState::kForeground) {
    for (auto& entry : foreground_send_surface_map_) {
      entry.second.OnShareTargetLost(share_target);
    }
  } else {
    for (auto& entry : background_send_surface_map_) {
      entry.second.OnShareTargetLost(share_target);
    }
  }
}

}  // namespace sharing
}  // namespace nearby
