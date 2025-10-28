// Copyright 2020 Google LLC
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

#ifndef PLATFORM_IMPL_WINDOWS_WIFI_HOTSPOT_H_
#define PLATFORM_IMPL_WINDOWS_WIFI_HOTSPOT_H_

// Windows headers
#include <windows.h>
#include <winsock2.h>
#include <wlanapi.h>

// Standard C/C++ headers
#include <cstddef>
#include <cstdint>
#include <exception>
#include <list>
#include <memory>
#include <string>
#include <utility>

// Nearby connections headers
#include "absl/base/thread_annotations.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/optional.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/implementation/wifi_hotspot.h"
#include "internal/platform/implementation/windows/scheduled_executor.h"
#include "internal/platform/implementation/windows/wifi_hotspot_native.h"
#include "internal/platform/implementation/windows/wifi_hotspot_server_socket.h"

// WinRT headers
#include "internal/platform/implementation/windows/generated/winrt/Windows.Devices.WiFiDirect.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Foundation.h"
#include "internal/platform/implementation/windows/generated/winrt/base.h"

#include "internal/platform/wifi_credential.h"

namespace nearby::windows {

using ::winrt::fire_and_forget;
using ::winrt::Windows::Devices::WiFiDirect::WiFiDirectAdvertisementPublisher;
using ::winrt::Windows::Devices::WiFiDirect::
    WiFiDirectAdvertisementPublisherStatusChangedEventArgs;
using ::winrt::Windows::Devices::WiFiDirect::WiFiDirectConnectionListener;
using ::winrt::Windows::Devices::WiFiDirect::
    WiFiDirectConnectionRequestedEventArgs;
using ::winrt::Windows::Devices::WiFiDirect::WiFiDirectDevice;

// Container of operations that can be performed over the WifiHotspot medium.
class WifiHotspotMedium : public api::WifiHotspotMedium {
 public:
  WifiHotspotMedium() = default;
  ~WifiHotspotMedium() override;

  // If the WiFi Adaptor supports to start a Hotspot interface.
  bool IsInterfaceValid() const override;

  // Discoverer connects to server socket
  std::unique_ptr<api::WifiHotspotSocket> ConnectToService(
      absl::string_view ip_address, int port,
      CancellationFlag* cancellation_flag) override;

  // Advertiser starts to listen on server socket
  std::unique_ptr<api::WifiHotspotServerSocket> ListenForService(
      int port) override;

  // Advertiser start WiFi Hotspot with specific Credentials.
  bool StartWifiHotspot(HotspotCredentials* hotspot_credentials) override;
  // Advertiser stop the current WiFi Hotspot
  bool StopWifiHotspot() override;
  // Discoverer connects to the Hotspot
  bool ConnectWifiHotspot(
      const HotspotCredentials& hotspot_credentials) override;
  // Discoverer disconnects from the Hotspot
  bool DisconnectWifiHotspot() override;

  absl::optional<std::pair<std::int32_t, std::int32_t>> GetDynamicPortRange()
      override {
    return absl::nullopt;
  }

 private:
  enum Value : char {
    kMediumStatusIdle = 0,
    kMediumStatusAccepting = (1 << 0),
    kMediumStatusBeaconing = (1 << 1),
    kMediumStatusConnected = (1 << 2),
  };

  bool IsIdle() { return medium_status_ == kMediumStatusIdle; }
  // Advertiser is accepting connection on server socket
  bool IsAccepting() { return (medium_status_ & kMediumStatusAccepting) != 0; }
  // Advertiser started Hotspot and sending beacon
  bool IsBeaconing() { return (medium_status_ & kMediumStatusBeaconing) != 0; }
  // Discoverer is connected with the Hotspot
  bool IsConnected() { return (medium_status_ & kMediumStatusConnected) != 0; }

  WiFiDirectAdvertisementPublisher publisher_{nullptr};
  WiFiDirectConnectionListener listener_{nullptr};

  // The list of WiFiDirectDevice is used to keep hotspot connection alive.
  std::list<WiFiDirectDevice> wifi_direct_devices_;

  fire_and_forget OnStatusChanged(
      WiFiDirectAdvertisementPublisher sender,
      WiFiDirectAdvertisementPublisherStatusChangedEventArgs event);
  winrt::event_token publisher_status_changed_token_;

  fire_and_forget OnConnectionRequested(
      WiFiDirectConnectionListener const& sender,
      WiFiDirectConnectionRequestedEventArgs const& event);
  winrt::event_token connection_requested_token_;

  // Gets error message from exception pointer
  std::string GetErrorMessage(std::exception_ptr eptr);

  // Protects to access some members
  absl::Mutex mutex_;

  // Medium Status
  int medium_status_ = kMediumStatusIdle;

  // Keep the server socket listener pointer
  WifiHotspotServerSocket* server_socket_ptr_ ABSL_GUARDED_BY(mutex_) = nullptr;

  // Scheduler for timeout.
  ScheduledExecutor scheduled_executor_;

  // connects Wi-Fi hotspot using native API.
  WifiHotspotNative wifi_hotspot_native_;
};

}  // namespace nearby::windows

#endif  // PLATFORM_IMPL_WINDOWS_WIFI_HOTSPOT_H_
