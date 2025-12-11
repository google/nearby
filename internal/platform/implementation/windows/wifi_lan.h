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
// clang-format on

// Standard C/C++ headers
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

// Nearby connections headers
#include "absl/base/nullability.h"
#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/cancelable.h"
#include "internal/platform/implementation/wifi_lan.h"
#include "internal/platform/implementation/windows/nearby_client_socket.h"
#include "internal/platform/implementation/windows/nearby_server_socket.h"
#include "internal/platform/implementation/windows/scheduled_executor.h"
#include "internal/platform/implementation/windows/socket_address.h"
#include "internal/platform/implementation/windows/wifi_lan_mdns.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/nsd_service_info.h"
#include "internal/platform/output_stream.h"
#include "internal/platform/service_address.h"
#include "internal/platform/wifi_credential.h"

// WinRT headers
#include "internal/platform/implementation/windows/generated/winrt/Windows.Devices.Enumeration.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Foundation.Collections.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Foundation.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Networking.ServiceDiscovery.Dnssd.h"
#include "internal/platform/implementation/windows/generated/winrt/base.h"

namespace nearby::windows {

// WifiLanSocket wraps the socket functions to read and write stream.
// In WiFi LAN, A WifiLanSocket will be passed to StartAcceptingConnections's
// call back when StreamSocketListener got connect. When call API to connect to
// remote WiFi LAN service, also will return a WifiLanSocket to caller.
class WifiLanSocket : public api::WifiLanSocket {
 public:
  WifiLanSocket();
  explicit WifiLanSocket(
      absl_nonnull std::unique_ptr<NearbyClientSocket> socket);
  WifiLanSocket(WifiLanSocket&&) = default;
  ~WifiLanSocket() override = default;
  WifiLanSocket& operator=(WifiLanSocket&&) = default;

  // Returns the InputStream of the WifiLanSocket.
  // On error, returned stream will report Exception::kIo on any operation.
  //
  // The returned object is not owned by the caller, and can be invalidated once
  // the WifiLanSocket object is destroyed.
  InputStream& GetInputStream() override { return input_stream_; };

  // Returns the OutputStream of the WifiLanSocket.
  // On error, returned stream will report Exception::kIo on any operation.
  //
  // The returned object is not owned by the caller, and can be invalidated once
  // the WifiLanSocket object is destroyed.
  OutputStream& GetOutputStream() override { return output_stream_; };

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  Exception Close() override { return client_socket_->Close(); };

  bool Connect(const SocketAddress& server_address, absl::Duration timeout) {
    return client_socket_->Connect(server_address, timeout);
  };

 private:
  // Internal properties
  absl_nonnull std::unique_ptr<NearbyClientSocket> client_socket_;
  SocketInputStream input_stream_;
  SocketOutputStream output_stream_;
};

// WifiLanServerSocket provides the support to server socket, this server socket
// accepts connection from clients.
class WifiLanServerSocket : public api::WifiLanServerSocket {
 public:
  WifiLanServerSocket() = default;
  WifiLanServerSocket(WifiLanServerSocket&&) = default;
  ~WifiLanServerSocket() override = default;
  WifiLanServerSocket& operator=(WifiLanServerSocket&&) = default;

  // Returns ip address.
  std::string GetIPAddress() const override;

  // Returns port.
  int GetPort() const override { return server_socket_.GetPort(); };

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
  void SetCloseNotifier(absl::AnyInvocable<void()> notifier) {
    server_socket_.SetCloseNotifier(std::move(notifier));
  };

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  Exception Close() override {
    server_socket_.Close();
    return {Exception::kSuccess};
  }

  // Binds to local port
  bool Listen(int port, bool dual_stack);

 private:
  NearbyServerSocket server_socket_;
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
      const ServiceAddress& service_address,
      CancellationFlag* cancellation_flag) override;

  std::unique_ptr<api::WifiLanServerSocket> ListenForService(int port) override;

  absl::optional<std::pair<std::int32_t, std::int32_t>> GetDynamicPortRange()
      override {
    return absl::nullopt;
  }

  std::vector<ServiceAddress> GetUpgradeAddressCandidates(
      const api::WifiLanServerSocket& server_socket) override;

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

  // Checks whether the service in the given timeout.
  // Returns true if the IP is connectable, otherwise return false.
  bool IsConnectableIpAddress(NsdServiceInfo& nsd_service_info,
                              absl::Duration timeout);

  std::unique_ptr<api::WifiLanSocket> ConnectToSocket(
      const SocketAddress& address, CancellationFlag* cancellation_flag,
      absl::Duration timeout);

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
      winrt::Windows::Foundation::Collections::IMapView<
          winrt::hstring, winrt::Windows::Foundation::IInspectable>
          properties,
      bool is_device_found);

  // mDNS callbacks for advertising and discovery
  winrt::fire_and_forget Watcher_DeviceAdded(
      winrt::Windows::Devices::Enumeration::DeviceWatcher sender,
      winrt::Windows::Devices::Enumeration::DeviceInformation deviceInfo);
  winrt::fire_and_forget Watcher_DeviceUpdated(
      winrt::Windows::Devices::Enumeration::DeviceWatcher sender,
      winrt::Windows::Devices::Enumeration::DeviceInformationUpdate
          deviceInfoUpdate);
  winrt::fire_and_forget Watcher_DeviceRemoved(
      winrt::Windows::Devices::Enumeration::DeviceWatcher sender,
      winrt::Windows::Devices::Enumeration::DeviceInformationUpdate
          deviceInfoUpdate);

  void RestartScanning();

  //
  // Dns-sd related properties
  //

  // Advertising properties
  winrt::Windows::Networking::ServiceDiscovery::Dnssd::DnssdServiceInstance
      dnssd_service_instance_{nullptr};
  winrt::Windows::Networking::ServiceDiscovery::Dnssd::DnssdRegistrationResult
      dnssd_regirstraion_result_{nullptr};

  // Discovery properties
  winrt::Windows::Devices::Enumeration::DeviceWatcher device_watcher_{nullptr};
  winrt::event_token device_watcher_added_event_token;
  winrt::event_token device_watcher_updated_event_token;
  winrt::event_token device_watcher_removed_event_token;

  // callback for discovery
  api::WifiLanMedium::DiscoveredServiceCallback discovered_service_callback_;

  // Medium Status
  int medium_status_ = kMediumStatusIdle;

  // The mDNS service name we are currently advertising.
  std::string service_name_;

  // mDNS service
  WifiLanMdns wifi_lan_mdns_;

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
};

}  // namespace nearby::windows

#endif  // PLATFORM_IMPL_WINDOWS_WIFI_LAN_H_
