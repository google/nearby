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

#include "location/nearby/sharing/lib/rpc/sharing_rpc_client.h"
#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/time/time.h"
#include "internal/base/observer_list.h"
#include "internal/platform/clock.h"
#include "sharing/advertisement.h"
#include "sharing/attachment_container.h"
#include "sharing/certificates/nearby_share_certificate_manager.h"
#include "sharing/internal/api/preference_manager.h"
#include "sharing/nearby_sharing_service.h"
#include "sharing/nearby_sharing_settings.h"
#include "sharing/share_target.h"
#include "sharing/share_target_discovered_callback.h"
#include "sharing/transfer_metadata.h"
#include "sharing/transfer_update_callback.h"
#include "sharing/wrapped_share_target_discovered_callback.h"
#include "internal/test/fake_clock.h"
#include "internal/test/fake_task_runner.h"
#include "location/nearby/sharing/lib/rpc/fake_nearby_share_client.h"
#include "location/nearby/sharing/lib/sync/sync_manager.h"
#include "sharing/analytics/analytics_recorder.h"
#include "sharing/fake_nearby_connections_manager.h"
#include "sharing/internal/test/fake_preference_manager.h"
#include "sharing/outgoing_targets_manager.h"

namespace nearby {
namespace sharing {

class FakeNearbySharingService : public NearbySharingService {
 public:
  FakeNearbySharingService();
  ~FakeNearbySharingService() override = default;

  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  // Shutdown the Nearby Sharing service, and cleanup.
  void Shutdown(
      std::function<void(StatusCodes)> status_codes_callback) override;

  // Register a send surface for handling payload transfer status and device.
  // discovery.
  void RegisterSendSurface(
      TransferUpdateCallback* transfer_callback,
      ShareTargetDiscoveredCallback* discovery_callback, SendSurfaceState state,
      Advertisement::BlockedVendorId blocked_vendor_id,
      bool disable_wifi_hotspot,
      absl::AnyInvocable<void(StatusCodes)> status_codes_callback) override;

  // Unregisters the current send surface.
  void UnregisterSendSurface(
      TransferUpdateCallback* transfer_callback,
      absl::AnyInvocable<void(StatusCodes)> status_codes_callback) override;

  // Registers a receiver surface for handling payload transfer status.
  void RegisterReceiveSurface(
      TransferUpdateCallback* transfer_callback, ReceiveSurfaceState state,
      Advertisement::BlockedVendorId vendor_id,
      absl::AnyInvocable<void(StatusCodes)> status_codes_callback) override;

  // Unregisters the current receive surface.
  void UnregisterReceiveSurface(
      TransferUpdateCallback* transfer_callback,
      absl::AnyInvocable<void(StatusCodes)> status_codes_callback) override;

  // Unregisters all foreground receive surfaces.
  void ClearForegroundReceiveSurfaces(
      absl::AnyInvocable<void(StatusCodes)> status_codes_callback) override;

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

  std::string Dump() const override;
  bool IsBluetoothPresent() const override { return true; }
  bool IsBluetoothPowered() const override { return true; }
  bool IsExtendedAdvertisingSupported() const override { return true; }
  bool IsLanConnected() const override { return true; }
  std::string GetQrCodeUrl() const override { return ""; }
  void SetVisibility(
      proto::DeviceVisibility visibility, absl::Duration expiration,
      absl::AnyInvocable<void(StatusCodes status_code) &&> callback) override {}
  void UpdateFilePathsInProgress(bool update_file_paths) override {}

  NearbyShareSettings* GetSettings() override;
  NearbyShareContactManager* GetContactManager() override;
  NearbyShareCertificateManager* GetCertificateManager() override;
  AccountManager* GetAccountManager() override;
  Clock& GetClock() override;
  void SetAlternateServiceUuidForDiscovery(
      uint16_t alternate_service_uuid) override {}
  SyncManager& sync_manager() override;
  OutgoingTargetsManager& outgoing_targets_manager() override;

  nearby::sharing::api::IdentityRpcClient& fake_identity_rpc_client() {
    return identity_rpc_client_;
  }
  nearby::sharing::api::PreferenceManager& fake_preference_manager() {
    return preference_manager_;
  }
  FakeNearbyConnectionsManager& fake_nearby_connections_manager() {
    return connections_manager_;
  }
  FakeTaskRunner& fake_task_runner() { return service_thread_; }
  analytics::AnalyticsRecorder& analytics_recorder() {
    return analytics_recorder_;
  }

  // Fake methods to support test scenarios.

  // Fire observer events.
  void FireHighVisibilityChangeRequested();
  void FireHighVisibilityChanged(bool in_high_visibility);
  void FireStartAdvertisingFailure();
  void FireStartDiscoveryResult(bool success);

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
  void FireShareTargetDiscovered(ShareTarget share_target);
  void FireShareTargetUpdated(ShareTarget share_target);
  void FireShareTargetLost(ShareTarget share_target);

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
  FakeClock clock_;
  FakeTaskRunner service_thread_;
  FakeNearbyConnectionsManager connections_manager_;
  analytics::AnalyticsRecorder analytics_recorder_;
  FakePreferenceManager preference_manager_;
  FakeNearbyIdentityClient identity_rpc_client_;
  std::unique_ptr<SyncManager> sync_manager_;
  std::unique_ptr<OutgoingTargetsManager> outgoing_targets_manager_;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_FAKE_NEARBY_SHARING_SERVICE_H_
