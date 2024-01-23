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
      ShareTargetDiscoveredCallback* discovery_callback,
      std::function<void(StatusCodes)> status_codes_callback) override;

  // Registers a receiver surface for handling payload transfer status.
  void RegisterReceiveSurface(
      TransferUpdateCallback* transfer_callback, ReceiveSurfaceState state,
      std::function<void(StatusCodes)> status_codes_callback) override;

  // Unregisters the current receive surface.
  void UnregisterReceiveSurface(
      TransferUpdateCallback* transfer_callback,
      std::function<void(StatusCodes)> status_codes_callback) override;

  // Unregisters all foreground receive surfaces.
  void ClearForegroundReceiveSurfaces(
      std::function<void(StatusCodes)> status_codes_callback) override;

  // Returns true if a foreground receive surface is registered.
  bool IsInHighVisibility() const override;

  // Returns true if there is an ongoing file transfer.
  bool IsTransferring() const override;

  // Returns true if we're currently receiving a file.
  bool IsReceivingFile() const override;

  // Returns true if we're currently sending a file.
  bool IsSendingFile() const override;

  // Returns true if we're currently attempting to connect to a
  // remote device.
  bool IsConnecting() const override;

  // Returns true if we are currently scanning for remote devices.
  bool IsScanning() const override;

  // Sends |attachments| to the remote |share_target|.
  void SendAttachments(
      const ShareTarget& share_target,
      std::vector<std::unique_ptr<Attachment>> attachments,
      std::function<void(StatusCodes)> status_codes_callback) override;

  // Accepts incoming share from the remote |share_target|.
  void Accept(const ShareTarget& share_target,
              std::function<void(StatusCodes status_codes)>
                  status_codes_callback) override;

  // Rejects incoming share from the remote |share_target|.
  void Reject(const ShareTarget& share_target,
              std::function<void(StatusCodes status_codes)>
                  status_codes_callback) override;

  // Cancels outgoing shares to the remote |share_target|.
  void Cancel(const ShareTarget& share_target,
              std::function<void(StatusCodes status_codes)>
                  status_codes_callback) override;

  // Returns true if the local user cancelled the transfer to remote
  // |share_target|.
  bool DidLocalUserCancelTransfer(const ShareTarget& share_target) override;

  // Opens attachments from the remote |share_target|.
  void Open(const ShareTarget& share_target,
            std::function<void(StatusCodes status_codes)> status_codes_callback)
      override;

  // Opens an url target on a browser instance.
  void OpenUrl(const ::nearby::network::Url& url) override;

  // Copies text to cache/clipboard.
  void CopyText(absl::string_view text) override;

  // Sets a cleanup callback to be called once done with transfer for ARC.
  void SetArcTransferCleanupCallback(std::function<void()> callback) override;

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
  void FireSendTransferUpdate(SendSurfaceState state, ShareTarget share_target,
                              TransferMetadata transfer_metadata);
  void FireReceiveTransferUpdate(ReceiveSurfaceState state,
                                 ShareTarget share_target,
                                 TransferMetadata transfer_metadata);

  // Fire discovery events.
  void FireShareTargetDiscovered(SendSurfaceState state,
                                 ShareTarget share_target);
  void FireShareTargetLost(SendSurfaceState state, ShareTarget share_target);

 private:
  ObserverList<Observer> observers_;
  ObserverList<NearbyShareSettings::Observer> settings_observers_;
  ObserverList<TransferUpdateCallback> foreground_send_transfer_callbacks_;
  ObserverList<TransferUpdateCallback> background_send_transfer_callbacks_;
  ObserverList<ShareTargetDiscoveredCallback>
      foreground_send_discovered_callbacks_;
  ObserverList<ShareTargetDiscoveredCallback>
      background_send_discovered_callbacks_;
  ObserverList<TransferUpdateCallback> foreground_receive_transfer_callbacks_;
  ObserverList<TransferUpdateCallback> background_receive_transfer_callbacks_;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_FAKE_NEARBY_SHARING_SERVICE_H_
