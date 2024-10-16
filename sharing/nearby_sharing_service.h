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

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "absl/base/attributes.h"
#include "absl/functional/any_invocable.h"
#include "absl/time/time.h"
#include "internal/platform/clock.h"
#include "sharing/advertisement.h"
#include "sharing/attachment_container.h"
#include "sharing/internal/api/sharing_rpc_notifier.h"
#include "sharing/local_device_data/nearby_share_local_device_data_manager.h"
#include "sharing/nearby_sharing_settings.h"
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
    // Method argument is invalid.
    // TODO(b/341292610): update dart side to use kInvalidArgument.
    // https://source.corp.google.com/piper///depot/google3/location/nearby/cpp/sharing/clients/dart/platform/lib/types/models.dart;rcl=637648200;l=21
    kInvalidArgument = 7,
    kMaxValue = kInvalidArgument
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
    enum class AdapterState {
      INVALID = 0,
      NOT_PRESENT = 1,
      DISABLED = 2,
      ENABLED = 3,
    };

    virtual ~Observer() = default;
    virtual void OnHighVisibilityChangeRequested() {}
    virtual void OnHighVisibilityChanged(bool in_high_visibility) = 0;

    virtual void OnStartAdvertisingFailure() {}
    virtual void OnStartDiscoveryResult(bool success) {}

    virtual void OnFastInitiationDevicesDetected() {}
    virtual void OnFastInitiationDevicesNotDetected() {}
    virtual void OnFastInitiationScanningStopped() {}

    virtual void OnBluetoothStatusChanged(AdapterState state) {}
    virtual void OnWifiStatusChanged(AdapterState state) {}
    virtual void OnLanStatusChanged(AdapterState state) {}
    virtual void OnIrrecoverableHardwareErrorReported() {}

    virtual void OnCredentialError() {}

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
  ABSL_DEPRECATED("Use the variant with vendor ID/blocking request instead.")
  virtual void RegisterSendSurface(
      TransferUpdateCallback* transfer_callback,
      ShareTargetDiscoveredCallback* discovery_callback, SendSurfaceState state,
      std::function<void(StatusCodes)> status_codes_callback) = 0;

  // Registers a send surface for handling payload transfer status and device
  // discovery, with optional blocking on a specified vendor ID.
  // `transfer_callback` is used as the main identity for the surface, so trying
  // to re-register the same transfer callback with a different
  // `discovery_callback` will result in an error delivered via the status
  // callback.
  // If `disable_wifi_hotspot` true, disables use of Wifi Hotspot for transfer
  // if the device is currently already connected to Wifi.  This prevents
  // interruption of connectivity on the device during transfers. If there are
  // multiple ongoing requests, any requests with `disable_wifi_hotspot` set to
  // true will disable use of Wifi Hotspot for the device.
  virtual void RegisterSendSurface(
      TransferUpdateCallback* transfer_callback,
      ShareTargetDiscoveredCallback* discovery_callback, SendSurfaceState state,
      Advertisement::BlockedVendorId blocked_vendor_id,
      bool disable_wifi_hotspot,
      std::function<void(StatusCodes)> status_codes_callback) = 0;

  // Unregisters the current send surface.
  virtual void UnregisterSendSurface(
      TransferUpdateCallback* transfer_callback,
      std::function<void(StatusCodes)> status_codes_callback) = 0;

  // Registers a receiver surface for handling payload transfer status, and
  // advertises the vendor ID specified by |vendor_id|.
  virtual void RegisterReceiveSurface(
      TransferUpdateCallback* transfer_callback, ReceiveSurfaceState state,
      Advertisement::BlockedVendorId vendor_id,
      std::function<void(StatusCodes)> status_codes_callback) = 0;

  // Unregisters the current receive surface.
  virtual void UnregisterReceiveSurface(
      TransferUpdateCallback* transfer_callback,
      std::function<void(StatusCodes)> status_codes_callback) = 0;

  // Unregisters all foreground receive surfaces.
  virtual void ClearForegroundReceiveSurfaces(
      std::function<void(StatusCodes)> status_codes_callback) = 0;

  // Returns true if there is an ongoing file transfer.
  virtual bool IsTransferring() const = 0;

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
      int64_t share_target_id,
      std::unique_ptr<AttachmentContainer> attachment_container,
      std::function<void(StatusCodes)> status_codes_callback) = 0;

  // Accepts incoming share from the remote |share_target|.
  virtual void Accept(
      int64_t share_target_id,
      std::function<void(StatusCodes status_codes)> status_codes_callback) = 0;

  // Rejects incoming share from the remote |share_target|.
  virtual void Reject(
      int64_t share_target_id,
      std::function<void(StatusCodes status_codes)> status_codes_callback) = 0;

  // Cancels outgoing shares to the remote |share_target|.
  virtual void Cancel(
      int64_t share_target_id,
      std::function<void(StatusCodes status_codes)> status_codes_callback) = 0;

  // Returns true if the local user cancelled the transfer to remote
  // |share_target|.
  virtual bool DidLocalUserCancelTransfer(int64_t share_target_id) = 0;

  // Checks to make sure visibility setting is valid and updates the service's
  // visibility if so.
  virtual void SetVisibility(
      proto::DeviceVisibility visibility, absl::Duration expiration,
      absl::AnyInvocable<void(StatusCodes status_code) &&> callback) = 0;

  virtual std::string Dump() const = 0;
  virtual void UpdateFilePathsInProgress(bool update_file_paths) = 0;

  virtual NearbyShareSettings* GetSettings() = 0;
  virtual nearby::sharing::api::SharingRpcNotifier* GetRpcNotifier() = 0;
  virtual NearbyShareLocalDeviceDataManager* GetLocalDeviceDataManager() = 0;
  virtual NearbyShareContactManager* GetContactManager() = 0;
  virtual NearbyShareCertificateManager* GetCertificateManager() = 0;
  virtual AccountManager* GetAccountManager() = 0;
  virtual Clock& GetClock() = 0;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_NEARBY_SHARING_SERVICE_H_
