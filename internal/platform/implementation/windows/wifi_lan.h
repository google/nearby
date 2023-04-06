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

#ifndef PLATFORM_IMPL_WINDOWS_WIFI_LAN_H_
#define PLATFORM_IMPL_WINDOWS_WIFI_LAN_H_

// Windows headers
// clang-format off
#include <windows.h>  // NOLINT
#include <windns.h>   // NOLINT
// clang-format on

// Standard C/C++ headers
#include <exception>
#include <memory>
#include <optional>
#include <string>

// Nearby connections headers
#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "internal/platform/cancellation_flag_listener.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/wifi_lan.h"
#include "internal/platform/implementation/windows/scheduled_executor.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/mutex.h"
#include "internal/platform/nsd_service_info.h"
#include "internal/platform/output_stream.h"
#include "internal/platform/scheduled_executor.h"

// WinRT headers
#include "internal/platform/implementation/windows/generated/winrt/Windows.Devices.Enumeration.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Foundation.Collections.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Foundation.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Networking.Connectivity.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Networking.ServiceDiscovery.Dnssd.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Networking.Sockets.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Storage.Streams.h"
#include "internal/platform/implementation/windows/generated/winrt/base.h"

namespace nearby {
namespace windows {

using winrt::fire_and_forget;
using winrt::Windows::Devices::Enumeration::DeviceInformation;
using winrt::Windows::Devices::Enumeration::DeviceInformationKind;
using winrt::Windows::Devices::Enumeration::DeviceInformationUpdate;
using winrt::Windows::Devices::Enumeration::DeviceWatcher;
using winrt::Windows::Devices::Enumeration::DeviceWatcherStatus;
using winrt::Windows::Foundation::IInspectable;
using winrt::Windows::Foundation::Collections::IMapView;
using winrt::Windows::Networking::HostName;
using winrt::Windows::Networking::HostNameType;
using winrt::Windows::Networking::Connectivity::NetworkInformation;
using winrt::Windows::Networking::ServiceDiscovery::Dnssd::
    DnssdRegistrationResult;
using winrt::Windows::Networking::ServiceDiscovery::Dnssd::
    DnssdRegistrationStatus;
using winrt::Windows::Networking::ServiceDiscovery::Dnssd::DnssdServiceInstance;
using winrt::Windows::Networking::Sockets::StreamSocket;
using winrt::Windows::Networking::Sockets::StreamSocketListener;
using winrt::Windows::Networking::Sockets::
    StreamSocketListenerConnectionReceivedEventArgs;
using winrt::Windows::Networking::Sockets::StreamSocketListenerInformation;
using winrt::Windows::Storage::Streams::Buffer;
using winrt::Windows::Storage::Streams::DataReader;
using winrt::Windows::Storage::Streams::IBuffer;
using winrt::Windows::Storage::Streams::IInputStream;
using winrt::Windows::Storage::Streams::InputStreamOptions;
using winrt::Windows::Storage::Streams::IOutputStream;

// WifiLanSocket wraps the socket functions to read and write stream.
// In WiFi LAN, A WifiLanSocket will be passed to StartAcceptingConnections's
// call back when StreamSocketListener got connect. When call API to connect to
// remote WiFi LAN service, also will return a WifiLanSocket to caller.
class WifiLanSocket : public api::WifiLanSocket {
 public:
  explicit WifiLanSocket(StreamSocket socket);
  WifiLanSocket(WifiLanSocket&) = default;
  WifiLanSocket(WifiLanSocket&&) = default;
  ~WifiLanSocket() override;
  WifiLanSocket& operator=(const WifiLanSocket&) = default;
  WifiLanSocket& operator=(WifiLanSocket&&) = default;

  // Returns the InputStream of the WifiLanSocket.
  // On error, returned stream will report Exception::kIo on any operation.
  //
  // The returned object is not owned by the caller, and can be invalidated once
  // the WifiLanSocket object is destroyed.
  InputStream& GetInputStream() override;

  // Returns the OutputStream of the WifiLanSocket.
  // On error, returned stream will report Exception::kIo on any operation.
  //
  // The returned object is not owned by the caller, and can be invalidated once
  // the WifiLanSocket object is destroyed.
  OutputStream& GetOutputStream() override;

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  Exception Close() override;

 private:
  // A simple wrapper to handle input stream of socket
  class SocketInputStream : public InputStream {
   public:
    SocketInputStream(IInputStream input_stream);
    ~SocketInputStream() = default;

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
    ~SocketOutputStream() = default;

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

// WifiLanServerSocket provides the support to server socket, this server socket
// accepts connection from clients.
class WifiLanServerSocket : public api::WifiLanServerSocket {
 public:
  explicit WifiLanServerSocket(int port = 0);
  WifiLanServerSocket(WifiLanServerSocket&) = default;
  WifiLanServerSocket(WifiLanServerSocket&&) = default;
  ~WifiLanServerSocket() override;
  WifiLanServerSocket& operator=(const WifiLanServerSocket&) = default;
  WifiLanServerSocket& operator=(WifiLanServerSocket&&) = default;

  // Returns ip address.
  std::string GetIPAddress() const override;

  // Returns port.
  int GetPort() const override;

  // Sets port
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
  std::unique_ptr<api::WifiLanSocket> Accept() override;

  // Called by the server side of a connection before passing ownership of
  // WifiLanServerSocker to user, to track validity of a pointer to this
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
  int port_ = 0;
  bool closed_ = false;
};

// Container of operations that can be performed over the WifiLan medium.
class WifiLanMedium : public api::WifiLanMedium {
 public:
  ~WifiLanMedium() override = default;

  // Check if a network connection to a primary router exist.
  bool IsNetworkConnected() const override;

  // Starts to advertising
  bool StartAdvertising(const NsdServiceInfo& nsd_service_info) override;

  // Stops to advertising
  bool StopAdvertising(const NsdServiceInfo& nsd_service_info) override;

  // Starts to discovery
  bool StartDiscovery(const std::string& service_type,
                      DiscoveredServiceCallback callback) override;

  // Returns true once WifiLan discovery for service_type is well and truly
  // stopped; after this returns, there must be no more invocations of the
  // DiscoveredServiceCallback passed in to StartDiscovery() for service_type.
  bool StopDiscovery(const std::string& service_type) override;

  std::unique_ptr<api::WifiLanSocket> ConnectToService(
      const NsdServiceInfo& remote_service_info,
      CancellationFlag* cancellation_flag) override;

  std::unique_ptr<api::WifiLanSocket> ConnectToService(
      const std::string& ip_address, int port,
      CancellationFlag* cancellation_flag) override;

  std::unique_ptr<api::WifiLanServerSocket> ListenForService(
      int port = 0) override;

  // DnsServiceDeRegister is a async process, after operation finish, callback
  // will call this method to notify the waiting method StopAdvertising to
  // continue.
  void NotifyDnsServiceUnregistered(DWORD status);

  absl::optional<std::pair<std::int32_t, std::int32_t>> GetDynamicPortRange()
      override {
    return absl::nullopt;
  }

 private:
  // Nsd status
  static const int kMediumStatusIdle = 0;
  static const int kMediumStatusAdvertising = (1 << 0);
  static const int kMediumStatusDiscovering = (1 << 1);

  // In the class, not using ENUM to describe the mDNS states, because a little
  // complicate to combine all states based on accepting, advertising and
  // discovery.
  bool IsIdle() { return medium_status_ == 0; }

  bool IsAdvertising() {
    return (medium_status_ & kMediumStatusAdvertising) != 0;
  }

  bool IsDiscovering() {
    return (medium_status_ & kMediumStatusDiscovering) != 0;
  }

  // Checks whether the IP address is connectable in the given timeout.
  // Parameters:
  //   ip - IP string in format as 192.168.1.1
  //   port - The IP port to connect.
  //   timeout - IP is not connectable if cannot connect in the duration.
  // Result - return true if the IP is connectable, otherwise return false.
  bool IsConnectableIpAddress(absl::string_view ip, int port,
                              absl::Duration timeout = absl::Seconds(1));

  // Methods to manage discovred services.
  void ClearDiscoveredServices() ABSL_LOCKS_EXCLUDED(mutex_);
  std::optional<NsdServiceInfo> GetDiscoveredService(absl::string_view id)
      ABSL_LOCKS_EXCLUDED(mutex_);
  void UpdateDiscoveredService(absl::string_view id,
                               const NsdServiceInfo& nsd_service_info)
      ABSL_LOCKS_EXCLUDED(mutex_);
  void RemoveDiscoveredService(absl::string_view id)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // From mDNS device information, to build NsdServiceInfo.
  // the properties are from DeviceInformation and DeviceInformationUpdate.
  // The API gets IP addresses, service name and text attributes of mDNS
  // from these properties,
  ExceptionOr<NsdServiceInfo> GetNsdServiceInformation(
      IMapView<winrt::hstring, IInspectable> properties, bool is_device_found);

  // mDNS callbacks for advertising and discovery
  fire_and_forget Watcher_DeviceAdded(DeviceWatcher sender,
                                      DeviceInformation deviceInfo);
  fire_and_forget Watcher_DeviceUpdated(
      DeviceWatcher sender, DeviceInformationUpdate deviceInfoUpdate);
  fire_and_forget Watcher_DeviceRemoved(
      DeviceWatcher sender, DeviceInformationUpdate deviceInfoUpdate);
  static void Advertising_StopCompleted(DWORD Status, PVOID pQueryContext,
                                        PDNS_SERVICE_INSTANCE pInstance);

  // Gets error message from exception pointer
  std::string GetErrorMessage(std::exception_ptr eptr);

  void RestartScanning();

  //
  // Dns-sd related properties
  //

  // Advertising properties
  DnssdServiceInstance dnssd_service_instance_{nullptr};
  DnssdRegistrationResult dnssd_regirstraion_result_{nullptr};

  // Stop advertising properties
  DNS_SERVICE_INSTANCE dns_service_instance_{nullptr};
  DNS_SERVICE_REGISTER_REQUEST dns_service_register_request_;
  std::unique_ptr<std::wstring> dns_service_instance_name_{nullptr};
  std::unique_ptr<CountDownLatch> dns_service_stop_latch_;
  DWORD dns_service_stop_status_;

  // Discovery properties
  DeviceWatcher device_watcher_{nullptr};
  winrt::event_token device_watcher_added_event_token;
  winrt::event_token device_watcher_updated_event_token;
  winrt::event_token device_watcher_removed_event_token;

  // callback for discovery
  api::WifiLanMedium::DiscoveredServiceCallback discovered_service_callback_;

  // Medium Status
  int medium_status_ = kMediumStatusIdle;

  // Used to keep the service name is advertising.
  std::string service_name_;

  // Keep the server sockets listener pointer
  absl::flat_hash_map<int /* port number of the WifiLanServerSocket*/,
                      WifiLanServerSocket*>
      port_to_server_socket_map_;

  // Used to protect the access to mDNS instances and scanning related data.
  absl::Mutex mutex_;

  // Keeps the map from device id to service during scanning.
  absl::flat_hash_map<std::string, NsdServiceInfo> discovered_services_map_
      ABSL_GUARDED_BY(mutex_);

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

#endif  // PLATFORM_IMPL_WINDOWS_WIFI_LAN_H_
