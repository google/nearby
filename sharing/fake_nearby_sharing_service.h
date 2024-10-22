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

#ifndef THIRD_PARTY_NEARBY_SHARING_FAKE_NEARBY_SHARING_SERVICE_H_
#define THIRD_PARTY_NEARBY_SHARING_FAKE_NEARBY_SHARING_SERVICE_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "absl/container/flat_hash_map.h"
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

class FakeNearbySharingService : public NearbySharingService {
 public:
  ~FakeNearbySharingService() override = default;

  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  bool HasObserver(Observer* observer) override;

  // Shutdown the Nearby Sharing service, and cleanup.
  void Shutdown(
      std::function<void(StatusCodes)> status_codes_callback) override;

  // Register a send surface for handling payload transfer status and device.
  // discovery.
  void RegisterSendSurface(
      TransferUpdateCallback* transfer_callback,
      ShareTargetDiscoveredCallback* discovery_callback, SendSurfaceState state,
      std::function<void(StatusCodes)> status_codes_callback) override;

  // Unregisters the current send surface.
  void UnregisterSendSurface(
      TransferUpdateCallback* transfer_callback,
      std::function<void(StatusCodes)> status_codes_callback) override;

  // Registers a receiver surface for handling payload transfer status.
  void RegisterReceiveSurface(
      TransferUpdateCallback* transfer_callback, ReceiveSurfaceState state,
      Advertisement::BlockedVendorId vendor_id,
      std::function<void(StatusCodes)> status_codes_callback) override;

  // Unregisters the current receive surface.
  void UnregisterReceiveSurface(
      TransferUpdateCallback* transfer_callback,
      std::function<void(StatusCodes)> status_codes_callback) override;

  // Unregisters all foreground receive surfaces.
  void ClearForegroundReceiveSurfaces(
      std::function<void(StatusCodes)> status_codes_callback) override;

  // Returns true if there is an ongoing file transfer.
  bool IsTransferring() const override;

  // Returns true if we are currently scanning for remote devices.
  bool IsScanning() const override;

  // Sends |attachments| to the remote |share_target|.
  void SendAttachments(
      int64_t share_target_id,
      std::unique_ptr<AttachmentContainer> attachment_container,
      std::function<void(StatusCodes)> status_codes_callback) override;

  // Accepts incoming share from the remote |share_target|.
  void Accept(int64_t share_target_id,
              std::function<void(StatusCodes status_codes)>
                  status_codes_callback) override;

  // Rejects incoming share from the remote |share_target|.
  void Reject(int64_t share_target_id,
              std::function<void(StatusCodes status_codes)>
                  status_codes_callback) override;

  // Cancels outgoing shares to the remote |share_target|.
  void Cancel(int64_t share_target_id,
              std::function<void(StatusCodes status_codes)>
                  status_codes_callback) override;

  // Returns true if the local user cancelled the transfer to remote
  // |share_target|.
  bool DidLocalUserCancelTransfer(int64_t share_target_id) override;

  std::string Dump() const override;

  NearbyShareSettings* GetSettings() override;
  nearby::sharing::api::SharingRpcNotifier* GetRpcNotifier() override;
  NearbyShareLocalDeviceDataManager* GetLocalDeviceDataManager() override;
  NearbyShareContactManager* GetContactManager() override;
  NearbyShareCertificateManager* GetCertificateManager() override;
  AccountManager* GetAccountManager() override;

  // Fake methods to support test scenarios.

  // Fire observer events.
  void FireHighVisibilityChangeRequested();
  void FireHighVisibilityChanged(bool in_high_visibility);
  void FireStartAdvertisingFailure();
  void FireStartDiscoveryResult(bool success);
  void FireFastInitiationDevicesDetected();
  void FireFastInitiationDevicesNotDetected();
  void FireFastInitiationScanningStopped();
  void FireShutdown();

  // Fire transfer update events.
  void FireSendTransferUpdate(SendSurfaceState state,
                              const ShareTarget& share_target,
                              const AttachmentContainer& attachment_container,
                              TransferMetadata transfer_metadata);
  void FireReceiveTransferUpdate(
      ReceiveSurfaceState state, const ShareTarget& share_target,
      const AttachmentContainer& attachment_container,
      TransferMetadata transfer_metadata);

  // Fire discovery events.
  void FireShareTargetDiscovered(SendSurfaceState state,
                                 ShareTarget share_target);
  void FireShareTargetLost(SendSurfaceState state, ShareTarget share_target);

 private:
  ObserverList<Observer> observers_;
  ObserverList<NearbyShareSettings::Observer> settings_observers_;
  // A mapping of foreground transfer callbacks to foreground send surface data.
  absl::flat_hash_map<TransferUpdateCallback*,
                      WrappedShareTargetDiscoveredCallback>
      foreground_send_surface_map_;
  // A mapping of background transfer callbacks to background send surface data.
  absl::flat_hash_map<TransferUpdateCallback*,
                      WrappedShareTargetDiscoveredCallback>
      background_send_surface_map_;
  ObserverList<TransferUpdateCallback> foreground_receive_transfer_callbacks_;
  ObserverList<TransferUpdateCallback> background_receive_transfer_callbacks_;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_FAKE_NEARBY_SHARING_SERVICE_H_
