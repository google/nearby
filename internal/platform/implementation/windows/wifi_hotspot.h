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
#include <wlanapi.h>

// Standard C/C++ headers

#include <memory>
#include <optional>
#include <string>

// Nearby connections headers
#include "internal/platform/cancellation_flag_listener.h"
#include "internal/platform/implementation/wifi_hotspot.h"
#include "internal/platform/implementation/windows/scheduled_executor.h"

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

// WifiHotspotSocket wraps the socket functions to read and write stream.
// In WiFi HOTSPOT, A WifiHotspotSocket will be passed to
// StartAcceptingConnections's call back when StreamSocketListener got connect.
// When call API to connect to remote WiFi Hotspot service, also will return a
// WifiHotspotSocket to caller.
class WifiHotspotSocket : public api::WifiHotspotSocket {
 public:
  explicit WifiHotspotSocket(StreamSocket socket);
  WifiHotspotSocket(const WifiHotspotSocket&) = default;
  WifiHotspotSocket(WifiHotspotSocket&&) = default;
  ~WifiHotspotSocket() override;
  WifiHotspotSocket& operator=(const WifiHotspotSocket&) = default;
  WifiHotspotSocket& operator=(WifiHotspotSocket&&) = default;

  // Returns the InputStream of the WifiHotspotSocket.
  // On error, returned stream will report Exception::kIo on any operation.
  //
  // The returned object is not owned by the caller, and can be invalidated once
  // the WifiHotspotSocket object is destroyed.
  InputStream& GetInputStream() override;

  // Returns the OutputStream of the WifiHotspotSocket.
  // On error, returned stream will report Exception::kIo on any operation.
  //
  // The returned object is not owned by the caller, and can be invalidated once
  // the WifiHotspotSocket object is destroyed.
  OutputStream& GetOutputStream() override;

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  Exception Close() override;

 private:
  // A simple wrapper to handle input stream of socket
  class SocketInputStream : public InputStream {
   public:
    SocketInputStream(IInputStream input_stream);
    ~SocketInputStream() override = default;

    ExceptionOr<ByteArray> Read(std::int64_t size) override;
    ExceptionOr<size_t> Skip(size_t offset) override;
    Exception Close() override;

   private:
    IInputStream input_stream_{nullptr};
  };

  // A simple wrapper to handle output stream of socket
  class SocketOutputStream : public OutputStream {
   public:
    SocketOutputStream(IOutputStream output_stream);
    ~SocketOutputStream() override = default;

    Exception Write(const ByteArray& data) override;
    Exception Flush() override;
    Exception Close() override;

   private:
    IOutputStream output_stream_{nullptr};
  };

  // Internal properties
  StreamSocket stream_soket_{nullptr};
  SocketInputStream input_stream_{nullptr};
  SocketOutputStream output_stream_{nullptr};
};

// WifiHotspotServerSocket provides the support to server socket, this server
// socket accepts connection from clients.
class WifiHotspotServerSocket : public api::WifiHotspotServerSocket {
 public:
  explicit WifiHotspotServerSocket(int port = 0);
  WifiHotspotServerSocket(const WifiHotspotServerSocket&) = default;
  WifiHotspotServerSocket(WifiHotspotServerSocket&&) = default;
  ~WifiHotspotServerSocket() override;
  WifiHotspotServerSocket& operator=(const WifiHotspotServerSocket&) = default;
  WifiHotspotServerSocket& operator=(WifiHotspotServerSocket&&) = default;

  std::string GetIPAddress() const override;
  int GetPort() const override;
  void SetPort(int port) { port_ = port; }

  StreamSocketListener GetSocketListener() const {
    return stream_socket_listener_;
  }

  // Blocks until either:
  // - at least one incoming connection request is available, or
  // - ServerSocket is closed.
  // On success, returns connected socket, ready to exchange data.
  // Returns nullptr on error.
  // Once error is reported, it is permanent, and ServerSocket has to be closed.
  std::unique_ptr<api::WifiHotspotSocket> Accept() override;

  // Called by the server side of a connection before passing ownership of
  // WifiHotspotServerSocker to user, to track validity of a pointer to this
  // server socket.
  void SetCloseNotifier(absl::AnyInvocable<void()> notifier);

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  Exception Close() override;

  // Binds to local port
  bool listen();

 private:
  // The listener is accepting incoming connections
  fire_and_forget Listener_ConnectionReceived(
      StreamSocketListener listener,
      StreamSocketListenerConnectionReceivedEventArgs const& args);

  // Retrieves IP addresses from local machine
  std::vector<std::string> GetIpAddresses() const;
  std::string GetHotspotIpAddresses() const;

  mutable absl::Mutex mutex_;
  absl::CondVar cond_;

  std::deque<StreamSocket> pending_sockets_ ABSL_GUARDED_BY(mutex_);
  StreamSocketListener stream_socket_listener_{nullptr};
  winrt::event_token listener_event_token_{};

  // Close notifier
  absl::AnyInvocable<void()> close_notifier_ = nullptr;

  // IP addresses of the computer. mDNS uses them to advertise.
  std::vector<std::string> ip_addresses_{};

  // Cache socket not be picked by upper layer
  std::string hotspot_ipaddr_ = {};
  int port_ = 0;
  bool closed_ = false;
};

// Container of operations that can be performed over the WifiHotspot medium.
class WifiHotspotMedium : public api::WifiHotspotMedium {
 public:
  WifiHotspotMedium();
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
  bool StartWifiHotspot(HotspotCredentials* hotspot_credentials_) override;
  // Advertiser stop the current WiFi Hotspot
  bool StopWifiHotspot() override;
  // Discoverer connects to the Hotspot
  bool ConnectWifiHotspot(HotspotCredentials* hotspot_credentials_) override;
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

  // Implemented the disconnection to WiFi hotspot, and used to avoid deadlock.
  bool InternalDisconnectWifiHotspot();

  bool IsIdle() { return medium_status_ == kMediumStatusIdle; }
  // Advertiser is accepting connection on server socket
  bool IsAccepting() { return (medium_status_ & kMediumStatusAccepting) != 0; }
  // Advertiser started Hotspot and sending beacon
  bool IsBeaconing() { return (medium_status_ & kMediumStatusBeaconing) != 0; }
  // Discoverer is connected with the Hotspot
  bool IsConnected() { return (medium_status_ & kMediumStatusConnected) != 0; }
  void RestoreWifiConnection();

  WiFiDirectAdvertisementPublisher publisher_{nullptr};
  WiFiDirectConnectionListener listener_{nullptr};
  WiFiDirectDevice wifi_direct_device_{nullptr};

  fire_and_forget OnStatusChanged(
      WiFiDirectAdvertisementPublisher sender,
      WiFiDirectAdvertisementPublisherStatusChangedEventArgs event);
  winrt::event_token publisher_status_changed_token_;

  fire_and_forget OnConnectionRequested(
      WiFiDirectConnectionListener const& sender,
      WiFiDirectConnectionRequestedEventArgs const& event);
  winrt::event_token connection_requested_token_;

  WiFiAdapter wifi_adapter_{nullptr};
  WiFiAvailableNetwork wifi_connected_network_{nullptr};

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

  // Scheduled task for connection timeout.
  std::shared_ptr<api::Cancelable> connection_timeout_ = nullptr;

  // Listener to connect cancellation.
  std::unique_ptr<nearby::CancellationFlagListener>
      connection_cancellation_listener_ = nullptr;
};

}  // namespace windows
}  // namespace nearby

#endif  // PLATFORM_IMPL_WINDOWS_WIFI_HOTSPOT_H_
