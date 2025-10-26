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
#include <memory>
#include <string>
#include <utility>

// Nearby connections headers
#include "absl/base/nullability.h"
#include "absl/base/thread_annotations.h"
#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/optional.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/wifi_hotspot.h"
#include "internal/platform/implementation/windows/nearby_client_socket.h"
#include "internal/platform/implementation/windows/nearby_server_socket.h"
#include "internal/platform/implementation/windows/scheduled_executor.h"
#include "internal/platform/implementation/windows/socket_address.h"
#include "internal/platform/implementation/windows/wifi_hotspot_native.h"

// WinRT headers
#include "internal/platform/implementation/windows/generated/winrt/Windows.Devices.WiFiDirect.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Foundation.h"
#include "internal/platform/implementation/windows/generated/winrt/base.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/output_stream.h"
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

// WifiHotspotSocket wraps the socket functions to read and write stream.
// In WiFi HOTSPOT, A WifiHotspotSocket will be passed to
// StartAcceptingConnections's callback when Winsock Server Socket receives a
// new connection. When call API to connect to remote WiFi Hotspot service, also
// will return a WifiHotspotSocket to caller.
class WifiHotspotSocket : public api::WifiHotspotSocket {
 public:
  WifiHotspotSocket();
  explicit WifiHotspotSocket(
      absl_nonnull std::unique_ptr<NearbyClientSocket> socket);
  WifiHotspotSocket(WifiHotspotSocket&&) = default;
  ~WifiHotspotSocket() override;
  WifiHotspotSocket& operator=(WifiHotspotSocket&&) = default;

  // Returns the InputStream of the WifiHotspotSocket.
  // On error, returned stream will report Exception::kIo on any operation.
  //
  // The returned object is not owned by the caller, and can be invalidated once
  // the WifiHotspotSocket object is destroyed.
  InputStream& GetInputStream() override { return input_stream_; }

  // Returns the OutputStream of the WifiHotspotSocket.
  // On error, returned stream will report Exception::kIo on any operation.
  //
  // The returned object is not owned by the caller, and can be invalidated once
  // the WifiHotspotSocket object is destroyed.
  OutputStream& GetOutputStream() override { return output_stream_; }

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  Exception Close() override { return client_socket_->Close(); }

  bool Connect(const SocketAddress& server_address) {
    return client_socket_->Connect(server_address);
  }

 private:
  // A simple wrapper to handle input stream of socket
  class SocketInputStream : public InputStream {
   public:
    explicit SocketInputStream(NearbyClientSocket* absl_nonnull client_socket)
        : client_socket_(client_socket) {}
    ~SocketInputStream() override = default;

    ExceptionOr<ByteArray> Read(std::int64_t size) override {
      return client_socket_->Read(size);
    }
    ExceptionOr<size_t> Skip(size_t offset) override {
      return client_socket_->Skip(offset);
    }
    Exception Close() override { return client_socket_->Close(); }

   private:
    NearbyClientSocket* absl_nonnull const client_socket_;
  };

  // A simple wrapper to handle output stream of socket
  class SocketOutputStream : public OutputStream {
   public:
    explicit SocketOutputStream(NearbyClientSocket* absl_nonnull client_socket)
        : client_socket_(client_socket) {}
    ~SocketOutputStream() override = default;

    Exception Write(const ByteArray& data) override {
      return client_socket_->Write(data);
    }
    Exception Flush() override { return client_socket_->Flush(); }
    Exception Close() override { return client_socket_->Close(); }

   private:
    NearbyClientSocket* absl_nonnull const client_socket_;
  };

  absl_nonnull std::unique_ptr<NearbyClientSocket> client_socket_;
  SocketInputStream input_stream_;
  SocketOutputStream output_stream_;
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
  bool Listen(bool dual_stack);

  NearbyServerSocket server_socket_;

 private:
  // Retrieves hotspot IP address from local machine
  std::string GetHotspotIpAddress() const;

  const int port_;
  mutable absl::Mutex mutex_;

  // Close notifier
  absl::AnyInvocable<void()> close_notifier_ = nullptr;

  // IP addresses of the computer. mDNS uses them to advertise.
  std::vector<std::string> ip_addresses_{};

  // Cache socket not be picked by upper layer
  std::string hotspot_ipaddr_ = {};
  bool closed_ = false;
};

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
