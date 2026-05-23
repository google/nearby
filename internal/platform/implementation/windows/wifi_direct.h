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
#include <wlanapi.h>

// Standard C/C++ headers
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// Nearby connections headers
#include "absl/base/nullability.h"
#include "absl/base/thread_annotations.h"
#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/optional.h"
#include "internal/platform/cancellation_flag.h"
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
#include "internal/platform/implementation/windows/generated/winrt/Windows.Devices.WiFiDirect.Services.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Foundation.Collections.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Foundation.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Networking.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Security.Credentials.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Security.Cryptography.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Storage.Streams.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.System.h"
#include "internal/platform/implementation/windows/generated/winrt/base.h"

namespace nearby::windows {

using ::winrt::event_token;
using ::winrt::fire_and_forget;
using ::winrt::Windows::Devices::Enumeration::DeviceInformation;
using ::winrt::Windows::Devices::Enumeration::DeviceInformationUpdate;
using ::winrt::Windows::Devices::Enumeration::DeviceWatcher;
using ::winrt::Windows::Devices::WiFiDirect::Services::WiFiDirectService;
using ::winrt::Windows::Devices::WiFiDirect::Services::
    WiFiDirectServiceAdvertisementStatus;
using ::winrt::Windows::Devices::WiFiDirect::Services::
    WiFiDirectServiceAdvertiser;
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

// WifiDirectSocket wraps the socket functions to read and write stream.
// On WiFiDirect GO serverside, a WifiDirectSocket will be passed to
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

class WifiDirectMedium : public api::WifiDirectMedium {
 public:
  WifiDirectMedium();
  ~WifiDirectMedium() override;
  // WifiDirectMedium is neither copyable nor movable.
  WifiDirectMedium(const WifiDirectMedium&) = delete;
  WifiDirectMedium& operator=(const WifiDirectMedium&) = delete;

  // If the WiFi Adaptor supports to start WifiDirect Service.
  bool IsInterfaceValid() const override;

  // If the WiFi Adaptor supports to start WifiDirect Service GO.
  // This is used to check if the Windows Firewall rule "WFD ASP Coordination
  // Protocol (UDP-In)" is enabled. GO interface is not valid if the rule is
  // disabled.
  bool IsGOInterfaceValid() const override;

  // Discoverer connects to server socket
  std::unique_ptr<api::WifiDirectSocket> ConnectToService(
      absl::string_view ip_address, int port,
      CancellationFlag* cancellation_flag) override;

  // Advertiser starts to listen on server socket
  std::unique_ptr<api::WifiDirectServerSocket> ListenForService(
      int port) override;

  // Advertiser start WiFiDirect GO with specific Credentials.
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
  enum Value : char {
    kMediumStatusIdle = 0,
    kMediumStatusAccepting = (1 << 0),
    kMediumStatusGOStarted = (1 << 1),
    kMediumStatusConnecting = (1 << 2),
    kMediumStatusConnected = (1 << 3),
  };
  // Medium Status
  int medium_status_ = kMediumStatusIdle;

  bool IsIdle() { return medium_status_ == kMediumStatusIdle; }
  // Advertiser is accepting connection on server socket
  bool IsAccepting() { return (medium_status_ & kMediumStatusAccepting) != 0; }
  // Advertiser started WifiDirect GO
  bool IsGOStarted() {
    return (medium_status_ & kMediumStatusGOStarted) != 0;
  }
  // Discoverer is connecting with the WifiDirect
  bool IsConnecting() {
    return (medium_status_ & kMediumStatusConnecting) != 0;
  }
  // Discoverer is connected with the WifiDirect
  bool IsConnected() { return (medium_status_ & kMediumStatusConnected) != 0; }
  // Returns true if the WiFi Direct service is supported.
  static bool IsWifiDirectServiceSupported();
  // Returns true if the Windows Firewall rule "WFD ASP Coordination Protocol
  // (UDP-In)" is enabled.
  static bool IsFirewallWfdAspProtoEnabled();
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

  WifiDirectCredentials* credentials_go_ = nullptr;
  WifiDirectCredentials credentials_gc_;
  std::string ip_address_local_;
  std::string ip_address_remote_;

  absl::Mutex mutex_;
  absl::CondVar is_ip_address_ready_;
  // Keep the server socket listener pointer
  WifiDirectServerSocket* server_socket_ptr_ ABSL_GUARDED_BY(mutex_) = nullptr;
  SubmittableExecutor listener_executor_;
};

}  // namespace nearby::windows

#endif  // PLATFORM_IMPL_WINDOWS_WIFI_DIRECT_H_
