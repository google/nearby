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

#ifndef PLATFORM_IMPL_WINDOWS_WIFI_DIRECT_H_
#define PLATFORM_IMPL_WINDOWS_WIFI_DIRECT_H_

// Windows headers
#include <windows.h>
#include <wlanapi.h>

// Standard C/C++ headers
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

// Nearby connections headers
#include "internal/platform/implementation/wifi_direct.h"
#include "internal/platform/implementation/wifi_hotspot.h"
#include "internal/platform/implementation/windows/scheduled_executor.h"
#include "internal/platform/implementation/windows/wifi_hotspot.h"

// WinRT headers
#include "absl/types/optional.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Devices.Enumeration.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Devices.WiFi.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Devices.WiFiDirect.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Foundation.Collections.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Foundation.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Networking.Connectivity.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Networking.Sockets.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Security.Credentials.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Security.Cryptography.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Storage.Streams.h"
#include "internal/platform/implementation/windows/generated/winrt/base.h"

namespace location {
namespace nearby {
namespace windows {

using ::winrt::fire_and_forget;
using ::winrt::Windows::Devices::WiFiDirect::
    WiFiDirectAdvertisementListenStateDiscoverability;
using ::winrt::Windows::Devices::WiFiDirect::WiFiDirectAdvertisementPublisher;
using ::winrt::Windows::Devices::WiFiDirect::
    WiFiDirectAdvertisementPublisherStatus;
using ::winrt::Windows::Devices::WiFiDirect::
    WiFiDirectAdvertisementPublisherStatusChangedEventArgs;
using ::winrt::Windows::Devices::WiFiDirect::WiFiDirectConnectionListener;
using ::winrt::Windows::Devices::WiFiDirect::WiFiDirectConnectionRequest;
using ::winrt::Windows::Devices::WiFiDirect::
    WiFiDirectConnectionRequestedEventArgs;
using ::winrt::Windows::Devices::WiFiDirect::WiFiDirectConnectionStatus;
using ::winrt::Windows::Devices::WiFiDirect::WiFiDirectDevice;

using ::winrt::Windows::Devices::WiFi::WiFiAccessStatus;
using ::winrt::Windows::Devices::WiFi::WiFiAdapter;
using ::winrt::Windows::Devices::WiFi::WiFiAvailableNetwork;
using ::winrt::Windows::Devices::WiFi::WiFiConnectionStatus;
using ::winrt::Windows::Devices::WiFi::WiFiReconnectionKind;

using ::winrt::Windows::Devices::Enumeration::DeviceInformation;
using ::winrt::Windows::Devices::Enumeration::DeviceInformationPairing;

using ::winrt::Windows::Storage::Streams::Buffer;
using ::winrt::Windows::Storage::Streams::IInputStream;
using ::winrt::Windows::Storage::Streams::InputStreamOptions;
using ::winrt::Windows::Storage::Streams::IOutputStream;

using ::winrt::Windows::Security::Credentials::PasswordCredential;
using ::winrt::Windows::Security::Cryptography::CryptographicBuffer;

using ::winrt::Windows::Networking::HostName;
using ::winrt::Windows::Networking::HostNameType;
using ::winrt::Windows::Networking::Connectivity::ConnectionProfile;
using ::winrt::Windows::Networking::Connectivity::ConnectionProfileDeleteStatus;
using ::winrt::Windows::Networking::Connectivity::NetworkInformation;
using ::winrt::Windows::Networking::Sockets::StreamSocket;
using ::winrt::Windows::Networking::Sockets::StreamSocketListener;
using ::winrt::Windows::Networking::Sockets::
    StreamSocketListenerConnectionReceivedEventArgs;

// Container of operations that can be performed over the WifiDirect medium.
class WifiDirectMedium : public api::WifiDirectMedium {
 public:
  WifiDirectMedium() = default;
  ~WifiDirectMedium() override;

  // If the WiFi Adaptor supports to start a WifiDirect interface.
  bool IsInterfaceValid() const override;

  // WifiDirect GC connects to server socket
  std::unique_ptr<api::WifiHotspotSocket> ConnectToService(
      absl::string_view ip_address, int port,
      CancellationFlag* cancellation_flag) override;

  // WifiDirect GO starts to listen on server socket
  std::unique_ptr<api::WifiHotspotServerSocket> ListenForService(
      int port) override;

  // Start WifiDirect GO with specific Credentials.
  bool StartWifiDirect(HotspotCredentials* wifi_direct_credentials) override;
  // Stop the current WifiDirect GO
  bool StopWifiDirect() override;

  // WifiDirect GC connects to the GO
  bool ConnectWifiDirect(HotspotCredentials* wifi_direct_credentials) override;
  // WifiDirect GC disconnects to the GO
  bool DisconnectWifiDirect() override;

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

  // Implemented the internal disconnection to WifiDirect GO, and used to avoid
  // deadlock.
  bool InternalDisconnectWifiDirect();
  bool IsIdle() { return medium_status_ == kMediumStatusIdle; }
  // WifiDirect GO is accepting connection on server socket
  bool IsAccepting() { return (medium_status_ & kMediumStatusAccepting) != 0; }
  // WifiDirect GO is started and sending beacon
  bool IsBeaconing() { return (medium_status_ & kMediumStatusBeaconing) != 0; }
  // WifiDirect GC is connected with the GO
  bool IsConnected() { return (medium_status_ & kMediumStatusConnected) != 0; }
  void RestoreWifiConnection();
  // Gets error message from exception pointer
  std::string GetErrorMessage(std::exception_ptr eptr);

  fire_and_forget OnStatusChanged(
      WiFiDirectAdvertisementPublisher sender,
      WiFiDirectAdvertisementPublisherStatusChangedEventArgs event);

  fire_and_forget OnConnectionRequested(
      WiFiDirectConnectionListener const& sender,
      WiFiDirectConnectionRequestedEventArgs const& event);

  // WINRT related data member
  WiFiAdapter wifi_adapter_{nullptr};
  WiFiAvailableNetwork wifi_connected_network_{nullptr};
  WiFiDirectAdvertisementPublisher publisher_{nullptr};
  WiFiDirectConnectionListener listener_{nullptr};
  WiFiDirectDevice wifi_direct_device_{nullptr};
  winrt::event_token publisher_status_changed_token_;
  winrt::event_token connection_requested_token_;

  // Protects to access some members
  absl::Mutex mutex_;

  // Medium Status
  int medium_status_ = kMediumStatusIdle;

  // Keep the server socket listener pointer
  WifiHotspotServerSocket* server_socket_ptr_ ABSL_GUARDED_BY(mutex_) = nullptr;

  // Scheduler for timeout.
  ScheduledExecutor scheduled_executor_;

  // Scheduled task for connection timeout.
  std::shared_ptr<api::Cancelable> connection_timeout_ = nullptr;

  // Listener to connect cancellation.
  std::unique_ptr<location::nearby::CancellationFlagListener>
      connection_cancellation_listener_ = nullptr;
};

}  // namespace windows
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_IMPL_WINDOWS_WIFI_DIRECT_H_
