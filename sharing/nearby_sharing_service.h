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

#ifndef THIRD_PARTY_NEARBY_SHARING_NEARBY_SHARING_SERVICE_H_
#define THIRD_PARTY_NEARBY_SHARING_NEARBY_SHARING_SERVICE_H_

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "internal/network/url.h"
#include "sharing/attachment.h"
#include "sharing/internal/api/sharing_rpc_notifier.h"
#include "sharing/local_device_data/nearby_share_local_device_data_manager.h"
#include "sharing/nearby_sharing_settings.h"
#include "sharing/share_target.h"
#include "sharing/share_target_discovered_callback.h"
#include "sharing/transfer_update_callback.h"

namespace nearby {

class AccountManager;

namespace sharing {

class NearbyNotificationDelegate;
class NearbyShareCertificateManager;
class NearbyShareContactManager;
class NearbyShareHttpNotifier;

// This service implements Nearby Sharing on top of the Nearby Connections mojo.
// Currently, only single profile will be allowed to be bound at a time and only
// after the user has enabled Nearby Sharing in prefs.
class NearbySharingService {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused. If entries are added, kMaxValue
  // should be updated.
  enum class StatusCodes {
    // The operation was successful.
    kOk = 0,
    // The operation failed, without any more information.
    kError = 1,
    // The operation failed since it was called in an invalid order.
    kOutOfOrderApiCall = 2,
    // Tried to stop something that was already stopped.
    kStatusAlreadyStopped = 3,
    // Tried to register an opposite foreground surface in the midst of a
    // transfer or connection.
    // (Tried to register Send Surface when receiving a file or tried to
    // register Receive Surface when
    // sending a file.)
    kTransferAlreadyInProgress = 4,
    // There is no available connection medium to use.
    kNoAvailableConnectionMedium = 5,
    // Bluetooth or WiFi hardware ran into an irrecoverable state. User PC needs
    // to be restarted.
    kIrrecoverableHardwareError = 6,
    kMaxValue = kIrrecoverableHardwareError
  };

  enum class ReceiveSurfaceState {
    // Default, invalid state.
    kUnknown,
    // Background receive surface advertises only to contacts.
    kBackground,
    // Foreground receive surface advertises to everyone.
    kForeground,
  };

  enum class SendSurfaceState {
    // Default, invalid state.
    kUnknown,
    // Background send surface only listens to transfer update.
    kBackground,
    // Foreground send surface both scans and listens to transfer update.
    kForeground,
  };

  class Observer {
   public:
    virtual ~Observer() = default;
    virtual void OnHighVisibilityChangeRequested() {}
    virtual void OnHighVisibilityChanged(bool in_high_visibility) = 0;

    virtual void OnStartAdvertisingFailure() {}
    virtual void OnStartDiscoveryResult(bool success) {}

    virtual void OnFastInitiationDevicesDetected() {}
    virtual void OnFastInitiationDevicesNotDetected() {}
    virtual void OnFastInitiationScanningStopped() {}

    virtual void OnBluetoothStatusChanged() {}
    virtual void OnWifiStatusChanged() {}
    virtual void OnLanStatusChanged() {}
    virtual void OnIrrecoverableHardwareErrorReported() {}

    // Called during the |KeyedService| shutdown, but before everything has been
    // cleaned up. It is safe to remove any observers on this event.
    virtual void OnShutdown() = 0;
  };

  static std::string StatusCodeToString(StatusCodes status_code);

  virtual ~NearbySharingService() = default;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
  virtual bool HasObserver(Observer* observer) = 0;

  // Shutdown the Nearby Sharing service, and cleanup.
  virtual void Shutdown(
      std::function<void(StatusCodes)> status_codes_callback) = 0;

  // Registers a send surface for handling payload transfer status and device
  // discovery.
  virtual void RegisterSendSurface(
      TransferUpdateCallback* transfer_callback,
      ShareTargetDiscoveredCallback* discovery_callback, SendSurfaceState state,
      std::function<void(StatusCodes)> status_codes_callback) = 0;

  // Unregisters the current send surface.
  virtual void UnregisterSendSurface(
      TransferUpdateCallback* transfer_callback,
      ShareTargetDiscoveredCallback* discovery_callback,
      std::function<void(StatusCodes)> status_codes_callback) = 0;

  // Registers a receiver surface for handling payload transfer status.
  virtual void RegisterReceiveSurface(
      TransferUpdateCallback* transfer_callback, ReceiveSurfaceState state,
      std::function<void(StatusCodes)> status_codes_callback) = 0;

  // Unregisters the current receive surface.
  virtual void UnregisterReceiveSurface(
      TransferUpdateCallback* transfer_callback,
      std::function<void(StatusCodes)> status_codes_callback) = 0;

  // Unregisters all foreground receive surfaces.
  virtual void ClearForegroundReceiveSurfaces(
      std::function<void(StatusCodes)> status_codes_callback) = 0;

  // Returns true if a foreground receive surface is registered.
  virtual bool IsInHighVisibility() const = 0;

  // Returns true if there is an ongoing file transfer.
  virtual bool IsTransferring() const = 0;

  // Returns true if we're currently receiving a file.
  virtual bool IsReceivingFile() const = 0;

  // Returns true if we're currently sending a file.
  virtual bool IsSendingFile() const = 0;

  // Returns true if we're currently attempting to connect to a
  // remote device.
  virtual bool IsConnecting() const = 0;

  // Returns true if we are currently scanning for remote devices.
  virtual bool IsScanning() const = 0;

  // Returns true if the bluetooth adapter is present.
  virtual bool IsBluetoothPresent() const = 0;

  // Returns true if the bluetooth adapter is powered.
  virtual bool IsBluetoothPowered() const = 0;

  // Returns true if extended advertising is supported by the BLE adapter
  virtual bool IsExtendedAdvertisingSupported() const = 0;

  // Returns true if the PC is connected to LAN (wifi/ethernet).
  virtual bool IsLanConnected() const = 0;

  // Returns true if the Wi-Fi adapter is present.
  virtual bool IsWifiPresent() const = 0;

  // Returns true if the Wi-Fi adapter is powered.
  virtual bool IsWifiPowered() const = 0;

  // Returns the QR Code Url.
  virtual std::string GetQrCodeUrl() const = 0;

  // Sends |attachments| to the remote |share_target|.
  virtual void SendAttachments(
      const ShareTarget& share_target,
      std::vector<std::unique_ptr<Attachment>> attachments,
      std::function<void(StatusCodes)> status_codes_callback) = 0;

  // Accepts incoming share from the remote |share_target|.
  virtual void Accept(
      const ShareTarget& share_target,
      std::function<void(StatusCodes status_codes)> status_codes_callback) = 0;

  // Rejects incoming share from the remote |share_target|.
  virtual void Reject(
      const ShareTarget& share_target,
      std::function<void(StatusCodes status_codes)> status_codes_callback) = 0;

  // Cancels outgoing shares to the remote |share_target|.
  virtual void Cancel(
      const ShareTarget& share_target,
      std::function<void(StatusCodes status_codes)> status_codes_callback) = 0;

  // Returns true if the local user cancelled the transfer to remote
  // |share_target|.
  virtual bool DidLocalUserCancelTransfer(const ShareTarget& share_target) = 0;

  // Opens attachments from the remote |share_target|.
  virtual void Open(
      const ShareTarget& share_target,
      std::function<void(StatusCodes status_codes)> status_codes_callback) = 0;

  // Opens an url target on a browser instance.
  virtual void OpenUrl(const ::nearby::network::Url& url) = 0;

  // Copies text to cache/clipboard.
  virtual void CopyText(absl::string_view text) = 0;

  // Persists and joins the Wi-Fi network.
  virtual void JoinWifiNetwork(absl::string_view ssid,
                               absl::string_view password) = 0;

  // Sets a cleanup callback to be called once done with transfer for ARC.
  virtual void SetArcTransferCleanupCallback(
      std::function<void()> callback) = 0;

  virtual std::string Dump() const = 0;
  virtual void UpdateFilePathsInProgress(bool update_file_paths) = 0;

  virtual NearbyShareSettings* GetSettings() = 0;
  virtual nearby::sharing::api::SharingRpcNotifier* GetRpcNotifier() = 0;
  virtual NearbyShareLocalDeviceDataManager* GetLocalDeviceDataManager() = 0;
  virtual NearbyShareContactManager* GetContactManager() = 0;
  virtual NearbyShareCertificateManager* GetCertificateManager() = 0;
  virtual AccountManager* GetAccountManager() = 0;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_NEARBY_SHARING_SERVICE_H_
