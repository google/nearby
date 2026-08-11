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

#ifndef PLATFORM_IMPL_WINDOWS_WIFI_DIRECT_H_
#define PLATFORM_IMPL_WINDOWS_WIFI_DIRECT_H_

// Windows headers
#include <windows.h>
#include <winsock2.h>
#include <wlanapi.h>

// Standard C/C++ headers
#include <memory>
#include <optional>
#include <string>

// Nearby connections headers
#include "absl/base/nullability.h"
#include "absl/base/thread_annotations.h"
#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/optional.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/wifi_direct.h"
#include "internal/platform/implementation/windows/nearby_client_socket.h"
#include "internal/platform/implementation/windows/nearby_server_socket.h"
#include "internal/platform/implementation/windows/socket_address.h"
#include "internal/platform/implementation/windows/submittable_executor.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/output_stream.h"
#include "internal/platform/wifi_credential.h"

// WinRT headers
#include "internal/platform/implementation/windows/generated/winrt/Windows.Devices.Enumeration.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Devices.WiFiDirect.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Foundation.Collections.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Foundation.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Networking.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Security.Credentials.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Security.Cryptography.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Storage.Streams.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.System.h"
#include "internal/platform/implementation/windows/generated/winrt/base.h"

namespace nearby::windows {

// WifiDirectSocket wraps the socket functions to read and write stream.
// On WiFiDirect GO server side, a WifiDirectSocket will be passed to
// StartAcceptingConnections's callback when Winsock Server Socket receives a
// new connection. When client side call API to connect to remote WiFi
// WifiDirect GO service, it will return a WifiDirectServiceSocket to caller.
class WifiDirectSocket : public api::WifiDirectSocket {
 public:
  WifiDirectSocket();
  explicit WifiDirectSocket(
      absl_nonnull std::unique_ptr<NearbyClientSocket> socket);
  WifiDirectSocket(WifiDirectSocket&&) = default;
  ~WifiDirectSocket() override;
  WifiDirectSocket& operator=(WifiDirectSocket&&) = default;

  // Returns the InputStream of the WifiDirectSocket.
  // On error, returned stream will report Exception::kIo on any operation.
  //
  // The returned object is not owned by the caller, and can be invalidated once
  // the WifiDirectSocket object is destroyed.
  InputStream& GetInputStream() override { return input_stream_; }

  // Returns the OutputStream of the WifiDirectSocket.
  // On error, returned stream will report Exception::kIo on any operation.
  //
  // The returned object is not owned by the caller, and can be invalidated once
  // the WifiDirectSocket object is destroyed.
  OutputStream& GetOutputStream() override { return output_stream_; }

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  Exception Close() override { return client_socket_->Close(); }

  bool Connect(const SocketAddress& server_address) {
    return client_socket_->Connect(server_address, absl::InfiniteDuration());
  }

 private:
  absl_nonnull std::unique_ptr<NearbyClientSocket> client_socket_;
  SocketInputStream input_stream_;
  SocketOutputStream output_stream_;
};

#if defined(ENABLE_WIFI_DIRECT_GO)
// WifiDirectServerSocket provides the support to server socket, this
// server socket accepts connection from clients.
class WifiDirectServerSocket : public api::WifiDirectServerSocket {
 public:
  WifiDirectServerSocket() = default;
  ~WifiDirectServerSocket() override;
  WifiDirectServerSocket(WifiDirectServerSocket&&) = default;
  WifiDirectServerSocket& operator=(WifiDirectServerSocket&&) = default;

  std::string GetIPAddress() const override;

  int GetPort() const override { return server_socket_.GetPort(); }

  // Blocks until either:
  // - at least one incoming connection request is available, or
  // - ServerSocket is closed.
  // On success, returns connected socket, ready to exchange data.
  // Returns nullptr on error.
  // Once error is reported, it is permanent, and ServerSocket has to be closed.
  std::unique_ptr<api::WifiDirectSocket> Accept() override;

  // Called by the server side of a connection before passing ownership of
  // WifiDirectServerSocker to user, to track validity of a pointer to
  // this server socket.
  void SetCloseNotifier(absl::AnyInvocable<void()> notifier) {
    server_socket_.SetCloseNotifier(std::move(notifier));
  }

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  Exception Close() override;

  // Populates the provided `wifi_direct_credentials` with the IP address
  // and port of this server socket.
  void PopulateWifiDirectCredentials(
      WifiDirectCredentials& wifi_direct_credentials) override;

  void SetIPAddress(std::string ip_address);

  // Binds to local port
  bool Listen(int port);

 private:
  // Retrieves WifiDirect GO IP address from local machine
  std::string GetWifiDirectIpAddress() const;

  mutable absl::Mutex mutex_;
  absl::CondVar is_listen_ready_;
  bool is_listen_started_ ABSL_GUARDED_BY(mutex_) = false;

  // IP addresses of the server socket.
  std::string wifi_direct_ipaddr_ = {};
  bool closed_ = false;
  NearbyServerSocket server_socket_;
  bool server_socket_accepted_connection_ = false;
};
#endif  // ENABLE_WIFI_DIRECT_GO

class WifiDirectDeviceDiscovered {
 public:
  explicit WifiDirectDeviceDiscovered(
      const winrt::Windows::Devices::Enumeration::DeviceInformation&
          device_info);

  ~WifiDirectDeviceDiscovered() = default;
  WifiDirectDeviceDiscovered(WifiDirectDeviceDiscovered&&) = default;
  WifiDirectDeviceDiscovered& operator=(WifiDirectDeviceDiscovered&&) = default;

  std::string GetId() { return id_; }
  winrt::Windows::Devices::Enumeration::DeviceInformation
  GetDeviceInformation() {
    return windows_wifi_direct_device_;
  }

 private:
  winrt::Windows::Devices::Enumeration::DeviceInformation
      windows_wifi_direct_device_;
  std::string id_;
};

// Container of operations that can be performed over the WifiLan medium.
class WifiDirectMedium : public api::WifiDirectMedium {
 public:
  WifiDirectMedium();
  ~WifiDirectMedium() override;
  // WifiDirectMedium is neither copyable nor movable.
  WifiDirectMedium(const WifiDirectMedium&) = delete;
  WifiDirectMedium& operator=(const WifiDirectMedium&) = delete;

  // If the WiFi Adaptor supports to start WifiDirect Service GO.
  bool IsInterfaceValid() const override;

  // Discoverer connects to server socket
  std::unique_ptr<api::WifiDirectSocket> ConnectToService(
      absl::string_view ip_address, int port,
      CancellationFlag* cancellation_flag) override;

  // Advertiser starts to listen on server socket
  std::unique_ptr<api::WifiDirectServerSocket> ListenForService(
      int port) override;

  bool StartWifiDirect(WifiDirectCredentials* wifi_direct_credentials) override;
  // Advertiser stop the current WiFiDirect GO.
  bool StopWifiDirect() override;
  // Discoverer connects to the WifiDirect GO as GC.
  bool ConnectWifiDirect(
      const WifiDirectCredentials& wifi_direct_credentials) override;
  // Discoverer disconnects from the connected WifiDirect GO.
  bool DisconnectWifiDirect() override;

  absl::optional<std::pair<std::int32_t, std::int32_t>> GetDynamicPortRange()
      override {
    return absl::nullopt;
  }

  // Returns the supported WifiDirect auth types.
  std::vector<WifiDirectAuthType> GetSupportedWifiDirectAuthTypes()
      const override;

 private:
  // Medium status
  enum Value : char {
    kMediumStatusIdle = 0,
    kMediumStatusAccepting = (1 << 0),
    kMediumStatusBeaconing = (1 << 1),
    kMediumStatusConnecting = (1 << 2),
    kMediumStatusConnected = (1 << 3),
  };

  // GC is connecting to the GO
  bool IsConnecting() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
    return (medium_status_ & kMediumStatusConnecting) != 0;
  }
  // GC is connected to the GO
  bool IsConnected() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
    return (medium_status_ & kMediumStatusConnected) != 0;
  }

#if defined(ENABLE_WIFI_DIRECT_GO)
  // Advertiser is accepting connection on server socket
  bool IsAccepting() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
    return (medium_status_ & kMediumStatusAccepting) != 0;
  }
  // GO is started and sending beacon
  bool IsBeaconing() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
    return (medium_status_ & kMediumStatusBeaconing) != 0;
  }

  // Advertising properties (GO)
  winrt::Windows::Devices::WiFiDirect::WiFiDirectAdvertisementPublisher
      publisher_{nullptr};
  winrt::Windows::Devices::WiFiDirect::WiFiDirectConnectionListener listener_{
      nullptr};
  winrt::Windows::Devices::WiFiDirect::WiFiDirectDevice wifi_direct_device_{
      nullptr};

  winrt::fire_and_forget OnStatusChanged(
      winrt::Windows::Devices::WiFiDirect::WiFiDirectAdvertisementPublisher
          sender,
      winrt::Windows::Devices::WiFiDirect::
          WiFiDirectAdvertisementPublisherStatusChangedEventArgs event);
  winrt::event_token publisher_status_changed_token_;

  winrt::fire_and_forget OnConnectionRequested(
      winrt::Windows::Devices::WiFiDirect::WiFiDirectConnectionListener const&
          sender,
      winrt::Windows::Devices::WiFiDirect::
          WiFiDirectConnectionRequestedEventArgs const& event);
  winrt::event_token connection_requested_token_;

  bool IsAepPaired(winrt::hstring device_id);

  absl::flat_hash_map<winrt::hstring,
                      std::unique_ptr<WifiDirectDeviceDiscovered>>
      connection_requested_devices_by_id_;
  WifiDirectCredentials* credentials_go_ = nullptr;
  absl::CondVar is_ip_address_ready_;
  std::string remote_device_name_;
  WifiDirectServerSocket* server_socket_ptr_ ABSL_GUARDED_BY(mutex_) = nullptr;
  SubmittableExecutor listener_executor_;
#endif  // ENABLE_WIFI_DIRECT_GO

  // Discovery properties
  winrt::Windows::Devices::Enumeration::DeviceWatcher device_watcher_{nullptr};
  winrt::event_token device_watcher_added_event_token_;
  winrt::event_token device_watcher_updated_event_token_;
  winrt::event_token device_watcher_removed_event_token_;
  winrt::event_token device_watcher_enumeration_completed_event_token_;
  winrt::event_token device_watcher_stopped_event_token_;

  winrt::fire_and_forget Watcher_DeviceAdded(
      winrt::Windows::Devices::Enumeration::DeviceWatcher sender,
      winrt::Windows::Devices::Enumeration::DeviceInformation device_info);
  winrt::fire_and_forget Watcher_DeviceUpdated(
      winrt::Windows::Devices::Enumeration::DeviceWatcher sender,
      winrt::Windows::Devices::Enumeration::DeviceInformationUpdate
          deviceInfoUpdate);
  winrt::fire_and_forget Watcher_DeviceRemoved(
      winrt::Windows::Devices::Enumeration::DeviceWatcher sender,
      winrt::Windows::Devices::Enumeration::DeviceInformationUpdate
          deviceInfoUpdate);
  winrt::fire_and_forget Watcher_DeviceEnumerationCompleted(
      winrt::Windows::Devices::Enumeration::DeviceWatcher sender,
      winrt::Windows::Foundation::IInspectable inspectable);
  winrt::fire_and_forget Watcher_DeviceStopped(
      winrt::Windows::Devices::Enumeration::DeviceWatcher sender,
      winrt::Windows::Foundation::IInspectable inspectable);

  winrt::fire_and_forget OnPairingRequested(
      winrt::Windows::Devices::Enumeration::
          DeviceInformationCustomPairing const& sender,
      winrt::Windows::Devices::Enumeration::
          DevicePairingRequestedEventArgs const& e);
  void OnConnectionStatusChanged(
      winrt::Windows::Devices::WiFiDirect::WiFiDirectDevice const& sender,
      winrt::Windows::Foundation::IInspectable const& e);
  // IAsyncOperation<bool> RequestPairDeviceAsync(
  bool RequestPairDeviceAsync(
      winrt::Windows::Devices::Enumeration::DeviceInformationPairing pairing,
      int group_owner_intent,
      winrt::Windows::Devices::WiFiDirect::WiFiDirectConfigurationMethod
          config_method);

  void CleanUpDeviceWatcherLocked() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  std::unique_ptr<CountDownLatch> connection_latch_;
  mutable absl::Mutex mutex_;

  absl::flat_hash_map<winrt::hstring,
                      std::unique_ptr<WifiDirectDeviceDiscovered>>
      discovered_devices_by_id_ ABSL_GUARDED_BY(mutex_);

  const bool is_interface_valid_ = false;
  WifiDirectCredentials credentials_gc_ ABSL_GUARDED_BY(mutex_);
  std::string ip_address_local_ ABSL_GUARDED_BY(mutex_);
  std::string ip_address_remote_ ABSL_GUARDED_BY(mutex_);
  int medium_status_ ABSL_GUARDED_BY(mutex_) = kMediumStatusIdle;
};
}  // namespace nearby::windows

#endif  // PLATFORM_IMPL_WINDOWS_WIFI_DIRECT_H_
