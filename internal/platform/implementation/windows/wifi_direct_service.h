// Copyright 2025 Google LLC
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

#ifndef PLATFORM_IMPL_WINDOWS_WIFI_DIRECT_SERVICE_H_
#define PLATFORM_IMPL_WINDOWS_WIFI_DIRECT_SERVICE_H_

// Windows headers
#include <windows.h>
#include <wlanapi.h>

// Standard C/C++ headers
#include <memory>
#include <string>

// Nearby connections headers
#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"

// WinRT headers
#include "internal/platform/implementation/windows/generated/winrt/Windows.Devices.Enumeration.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Devices.WiFiDirect.Services.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Foundation.Collections.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Foundation.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Networking.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Security.Credentials.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Security.Cryptography.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Storage.Streams.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.System.h"
#include "internal/platform/implementation/windows/generated/winrt/base.h"

namespace nearby {
namespace windows {
using ::winrt::event_token;
using ::winrt::fire_and_forget;
using ::winrt::Windows::Devices::Enumeration::DeviceInformation;
using ::winrt::Windows::Devices::Enumeration::DeviceInformationUpdate;
using ::winrt::Windows::Devices::Enumeration::DeviceWatcher;
using ::winrt::Windows::Devices::WiFiDirect::Services::WiFiDirectService;
using ::winrt::Windows::Devices::WiFiDirect::Services::
    WiFiDirectServiceAdvertiser;
using ::winrt::Windows::Devices::WiFiDirect::Services::
    WiFiDirectServiceAdvertisementStatus;
using ::winrt::Windows::Devices::WiFiDirect::Services::
    WiFiDirectServiceAutoAcceptSessionConnectedEventArgs;
using ::winrt::Windows::Devices::WiFiDirect::Services::
    WiFiDirectServiceConfigurationMethod;
using ::winrt::Windows::Devices::WiFiDirect::Services::WiFiDirectServiceSession;
using ::winrt::Windows::Devices::WiFiDirect::Services::
    WiFiDirectServiceSessionRequestedEventArgs;
using ::winrt::Windows::Devices::WiFiDirect::Services::WiFiDirectServiceStatus;
using ::winrt::Windows::Foundation::AsyncStatus;
using ::winrt::Windows::Foundation::IInspectable;

class WifiDirectServiceDiscovered {
 public:
  explicit WifiDirectServiceDiscovered(const DeviceInformation& device_info);

  ~WifiDirectServiceDiscovered() = default;
  WifiDirectServiceDiscovered(WifiDirectServiceDiscovered&&) = default;
  WifiDirectServiceDiscovered& operator=(WifiDirectServiceDiscovered&&) =
      default;

  std::string GetId() { return id_; }
  DeviceInformation GetDeviceInformation() {
    return windows_wifi_direct_device_;
  }

 private:
  DeviceInformation windows_wifi_direct_device_;

  // Once the device is lost, we can no longer access it's id.
  std::string id_;

  // Once the device is lost, we can no longer access it's mac address.
  // std::string name_;
};

class WifiDirectServiceMedium {
 public:
  WifiDirectServiceMedium();
  ~WifiDirectServiceMedium();
  // WifiDirectServiceMedium is neither copyable nor movable.
  WifiDirectServiceMedium(const WifiDirectServiceMedium&) = delete;
  WifiDirectServiceMedium& operator=(const WifiDirectServiceMedium&) = delete;

  // Starts to advertising
  bool StartWifiDirectService();
  // Stops to advertising
  bool StopWifiDirectService();
  // Connects to a WifiDirectService
  bool ConnectWifiDirectService();
  // Disconnects from a WifiDirectService
  bool DisconnectWifiDirectService();

 private:
  enum Value : char {
    kMediumStatusIdle = 0,
    kMediumStatusAccepting = (1 << 0),
    kMediumStatusServiceStarted = (1 << 1),
    kMediumStatusConnecting = (1 << 2),
  };
  // Medium Status
  int medium_status_ = kMediumStatusIdle;

  bool IsIdle() { return medium_status_ == kMediumStatusIdle; }
  // Advertiser is accepting connection on server socket
  bool IsAccepting() { return (medium_status_ & kMediumStatusAccepting) != 0; }
  // Advertiser started Hotspot and sending beacon
  bool IsServiceStarted() {
    return (medium_status_ & kMediumStatusServiceStarted) != 0;
  }
  // Discoverer is connected with the Hotspot
  bool IsConnecting() {
    return (medium_status_ & kMediumStatusConnecting) != 0;
  }

  // Converts WiFiDirectServiceConfigurationMethod enum to a string.
  static std::string ConfigMethodToString(
      WiFiDirectServiceConfigurationMethod config_method);

  WiFiDirectServiceAdvertiser advertiser_ = nullptr;
  WiFiDirectService service_ = nullptr;
  WiFiDirectServiceSession session_ = nullptr;
  winrt::Windows::System::DispatcherQueueController controller_ = nullptr;
  winrt::Windows::System::DispatcherQueue dispatcher_queue_ = nullptr;
  DeviceInformation device_info_ = nullptr;

  fire_and_forget OnAdvertisementStatusChanged(
      WiFiDirectServiceAdvertiser sender, IInspectable const& event);
  fire_and_forget OnAutoAcceptSessionConnected(
      WiFiDirectServiceAdvertiser sender,
      WiFiDirectServiceAutoAcceptSessionConnectedEventArgs const& args);
  fire_and_forget OnSessionRequested(
      WiFiDirectServiceAdvertiser const& sender,
      WiFiDirectServiceSessionRequestedEventArgs const& args);

  event_token advertisement_status_changed_token_;
  event_token auto_accept_session_connected_token_;
  event_token session_requested_token_;

  // Discovery properties
  DeviceWatcher device_watcher_{nullptr};
  event_token device_watcher_added_event_token_;
  event_token device_watcher_updated_event_token_;
  event_token device_watcher_removed_event_token_;
  event_token device_watcher_enumeration_completed_event_token_;
  event_token device_watcher_stopped_event_token_;

  fire_and_forget Watcher_DeviceAdded(DeviceWatcher sender,
                                      DeviceInformation device_info);
  fire_and_forget Watcher_DeviceUpdated(
      DeviceWatcher sender, DeviceInformationUpdate deviceInfoUpdate);
  fire_and_forget Watcher_DeviceRemoved(
      DeviceWatcher sender, DeviceInformationUpdate deviceInfoUpdate);
  fire_and_forget Watcher_DeviceEnumerationCompleted(DeviceWatcher sender,
                                                     IInspectable inspectable);
  fire_and_forget Watcher_DeviceStopped(DeviceWatcher sender,
                                        IInspectable inspectable);
  std::string ip_address_local_;
  std::string ip_address_remote_;

  absl::Mutex mutex_;
  absl::flat_hash_map<winrt::hstring,
                      std::unique_ptr<WifiDirectServiceDiscovered>>
      discovered_devices_by_id_;

  absl::flat_hash_map<winrt::hstring,
                      std::unique_ptr<WifiDirectServiceDiscovered>>
      connection_requested_devices_by_id_;
};

}  // namespace windows
}  // namespace nearby

#endif  // PLATFORM_IMPL_WINDOWS_WIFI_DIRECT_SERVICE_H_
