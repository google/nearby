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

// Nearby connections headers
#include "absl/base/nullability.h"
#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/optional.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/wifi_direct.h"
#include "internal/platform/implementation/windows/nearby_client_socket.h"
#include "internal/platform/implementation/windows/nearby_server_socket.h"
#include "internal/platform/implementation/windows/submittable_executor.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/output_stream.h"

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
// In WiFi HOTSPOT, A WifiDirectSocket will be passed to
// StartAcceptingConnections's callback when Winsock Server Socket receives a
// new connection. When call API to connect to remote WiFi Hotspot service, also
// will return a WifiDirectSocket to caller.
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

// WifiDirectServerSocket provides the support to server socket, this
// server socket accepts connection from clients.
class WifiDirectServerSocket : public api::WifiDirectServerSocket {
 public:
  explicit WifiDirectServerSocket(int port = 0);
  WifiDirectServerSocket(const WifiDirectServerSocket&) = default;
  WifiDirectServerSocket(WifiDirectServerSocket&&) = default;
  ~WifiDirectServerSocket() override;
  WifiDirectServerSocket& operator=(const WifiDirectServerSocket&) = default;
  WifiDirectServerSocket& operator=(WifiDirectServerSocket&&) = default;

  std::string GetIPAddress() const override;
  int GetPort() const override;

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
  void SetCloseNotifier(absl::AnyInvocable<void()> notifier);

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  Exception Close() override;

  // Binds to local port
  bool Listen(bool dual_stack, std::string& ip_address);

  NearbyServerSocket server_socket_;

 private:
  // Retrieves hotspot IP address from local machine
  std::string GetWifiDirectIpAddress() const;

  const int port_;
  mutable absl::Mutex mutex_;

  // Close notifier
  absl::AnyInvocable<void()> close_notifier_ = nullptr;

  // IP addresses of the server socket.
  std::string wifi_direct_ipaddr_ = {};
  bool closed_ = false;
};

class WifiDirectDiscovered {
 public:
  explicit WifiDirectDiscovered(const DeviceInformation& device_info);

  ~WifiDirectDiscovered() = default;
  WifiDirectDiscovered(WifiDirectDiscovered&&) = default;
  WifiDirectDiscovered& operator=(WifiDirectDiscovered&&) = default;

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

class WifiDirectMedium {
 public:
  WifiDirectMedium();
  ~WifiDirectMedium();
  // WifiDirectMedium is neither copyable nor movable.
  WifiDirectMedium(const WifiDirectMedium&) = delete;
  WifiDirectMedium& operator=(const WifiDirectMedium&) = delete;

  // If the WiFi Adaptor supports to start WifiDirect Service GO.
  bool IsInterfaceValid() const;

  // Discoverer connects to server socket
  std::unique_ptr<api::WifiDirectSocket> ConnectToService(
      absl::string_view ip_address, int port,
      CancellationFlag* cancellation_flag);

  // Advertiser starts to listen on server socket
  std::unique_ptr<api::WifiDirectServerSocket> ListenForService(int port);

  // Starts to advertising
  bool StartWifiDirect();
  // Stops to advertising
  bool StopWifiDirect();
  // Connects to a WifiDirect
  bool ConnectWifiDirect();
  // Disconnects from a WifiDirect
  bool DisconnectWifiDirect();

  absl::optional<std::pair<std::int32_t, std::int32_t>> GetDynamicPortRange() {
    return absl::nullopt;
  }

 private:
  enum Value : char {
    kMediumStatusIdle = 0,
    kMediumStatusAccepting = (1 << 0),
    kMediumStatusServiceStarted = (1 << 1),
    kMediumStatusConnecting = (1 << 2),
    kMediumStatusConnected = (1 << 3),
  };
  // Medium Status
  int medium_status_ = kMediumStatusIdle;

  bool IsIdle() { return medium_status_ == kMediumStatusIdle; }
  // Advertiser is accepting connection on server socket
  bool IsAccepting() { return (medium_status_ & kMediumStatusAccepting) != 0; }
  // Advertiser started WifiDirect
  bool IsServiceStarted() {
    return (medium_status_ & kMediumStatusServiceStarted) != 0;
  }
  // Discoverer is connecting with the WifiDirect
  bool IsConnecting() {
    return (medium_status_ & kMediumStatusConnecting) != 0;
  }
  // Discoverer is connected with the WifiDirect
  bool IsConnected() { return (medium_status_ & kMediumStatusConnected) != 0; }

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
  absl::CondVar is_ip_address_ready_;
  // Keep the server socket listener pointer
  WifiDirectServerSocket* server_socket_ptr_ ABSL_GUARDED_BY(mutex_) = nullptr;
  SubmittableExecutor listener_executor_;

  absl::flat_hash_map<winrt::hstring, std::unique_ptr<WifiDirectDiscovered>>
      discovered_devices_by_id_;

  absl::flat_hash_map<winrt::hstring, std::unique_ptr<WifiDirectDiscovered>>
      connection_requested_devices_by_id_;
};

}  // namespace nearby::windows

#endif  // PLATFORM_IMPL_WINDOWS_WIFI_DIRECT_H_
