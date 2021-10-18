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
#include <windows.h>       // NOLINT
#include <win32/windns.h>  // NOLINT

// Standard C/C++ headers
#include <exception>
#include <memory>
#include <string>

// Nearby connections headers
#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/synchronization/mutex.h"
#include "platform/api/wifi_lan.h"
#include "platform/base/exception.h"
#include "platform/base/input_stream.h"
#include "platform/base/output_stream.h"
#include "platform/public/count_down_latch.h"
#include "platform/public/mutex.h"

// WinRT headers
#include "platform/impl/windows/generated/winrt/Windows.Devices.Enumeration.h"
#include "platform/impl/windows/generated/winrt/Windows.Foundation.Collections.h"
#include "platform/impl/windows/generated/winrt/Windows.Foundation.h"
#include "platform/impl/windows/generated/winrt/Windows.Networking.Connectivity.h"
#include "platform/impl/windows/generated/winrt/Windows.Networking.ServiceDiscovery.Dnssd.h"
#include "platform/impl/windows/generated/winrt/Windows.Networking.Sockets.h"
#include "platform/impl/windows/generated/winrt/Windows.Storage.Streams.h"
#include "platform/impl/windows/generated/winrt/base.h"

namespace location {
namespace nearby {
namespace windows {

using winrt::fire_and_forget;
using winrt::Windows::Devices::Enumeration::DeviceInformation;
using winrt::Windows::Devices::Enumeration::DeviceInformationKind;
using winrt::Windows::Devices::Enumeration::DeviceInformationUpdate;
using winrt::Windows::Devices::Enumeration::DeviceWatcher;
using winrt::Windows::Foundation::IInspectable;
using winrt::Windows::Foundation::Collections::IMapView;
using winrt::Windows::Networking::HostName;
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

class WifiLanMedium;

// WifiLanService includes NSD service information and
// related medium information.
class WifiLanService : public api::WifiLanService {
 public:
  WifiLanService() = default;
  explicit WifiLanService(NsdServiceInfo nsd_service_info)
      : nsd_service_info_(std::move(nsd_service_info)) {}
  ~WifiLanService() override = default;

  NsdServiceInfo GetServiceInfo() const override { return nsd_service_info_; }

  void SetServiceInfo(NsdServiceInfo nsd_service_info) {
    nsd_service_info_ = std::move(nsd_service_info);
  }

  WifiLanMedium* GetMedium() { return medium_; }

  void SetMedium(WifiLanMedium* medium) { medium_ = medium; }

 private:
  NsdServiceInfo nsd_service_info_;
  WifiLanMedium* medium_ = nullptr;
};

// WifiLanSocket wraps the socket functions to read and write stream.
// In WiFi LAN, A WifiLanSocket will be passed to StartAcceptingConnections's
// call back when StreamSocketListener got connect. When call API to connect to
// remote WiFi LAN service, also will return a WifiLanSocket to caller.
class WifiLanSocket : public api::WifiLanSocket {
 public:
  explicit WifiLanSocket(api::WifiLanService* wifi_lan_service,
                         StreamSocket socket);
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

  // Returns valid WifiLanService pointer if there is a connection, and
  // nullptr otherwise.
  api::WifiLanService* GetRemoteWifiLanService() override;

  // Sets service id binding to the socket
  void SetServiceId(std::string service_id);

  // Sets medium information
  void SetMedium(WifiLanMedium* medium);

  // Returns the socket IP address
  std::string GetLocalAddress();

  // Returns the socket port, range is between 49152 and 65535
  int GetLocalPort();

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

  api::WifiLanService* remote_wifi_lan_service_ = nullptr;
  WifiLanMedium* medium_ = nullptr;
  std::string service_id_;
};

// WifiLanNsd implements the NSD functions for a specific service ID.
// WifiLan Medium separates NSD functions using WifiLanNsd. WifiLanNsd
// maintians the states of mDNS service.
class WifiLanNsd {
 public:
  explicit WifiLanNsd(WifiLanMedium* medium, const std::string service_id);
  WifiLanNsd(WifiLanNsd&&) = default;
  WifiLanNsd& operator=(WifiLanNsd&&) = default;
  ~WifiLanNsd() = default;

  // Implements medium functions based on service id
  bool StartAcceptingConnections(
      api::WifiLanMedium::AcceptedConnectionCallback callback);
  bool StopAcceptingConnections();
  bool StartAdvertising(const NsdServiceInfo& nsd_service_info);
  bool StopAdvertising();
  bool StartDiscovery(api::WifiLanMedium::DiscoveredServiceCallback callback);
  bool StopDiscovery();

  // In the class, not using ENUM to describe the mDNS states, because a little
  // complicate to combine all states based on accepting, advertising and
  // discovery.
  bool IsIdle() { return nsd_status_ == 0; }

  bool IsAccepting() { return (nsd_status_ & NSD_STATUS_ACCEPTING) != 0; }

  bool IsAdvertising() { return (nsd_status_ & NSD_STATUS_ADVERTISING) != 0; }

  bool IsDiscovering() { return (nsd_status_ & NSD_STATUS_DISCOVERING) != 0; }

  // A pair of IP Address and Port. A remote device can use this information
  // to connect to us. This is non-null while IsAccepting is true.
  std::pair<std::string, int> GetCredentials();

  // DnsServiceDeRegister is a async process, after operation finish, callback
  // will call this method to notify the waiting method StopAdvertising to
  // continue.
  void NotifyDnsServiceUnregistered(DWORD status);

 private:
  // Nsd status
  static const int NSD_STATUS_IDLE = 0;
  static const int NSD_STATUS_ACCEPTING = (1 << 0);
  static const int NSD_STATUS_ADVERTISING = (1 << 1);
  static const int NSD_STATUS_DISCOVERING = (1 << 2);

  //
  // Constants
  //

  // Socket listening ports
  static const uint16 PORT_MIN = 49152;
  static const uint16 PORT_MAX = 65535;
  static const uint16 PORT_RANGE = PORT_MAX - PORT_MIN;

  // mDNS text attributes
  static constexpr std::string_view KEY_ENDPOINT_INFO = "n";

  // mDNS information for advertising and discovery
  static constexpr std::wstring_view MDNS_HOST_NAME = L"Windows.local";
  static constexpr std::string_view MDNS_INSTANCE_NAME_FORMAT =
      "%s.%s._tcp.local";
  static constexpr std::string_view MDNS_DEVICE_SELECTOR_FORMAT =
      "System.Devices.AepService.ProtocolId:=\"{4526e8c1-8aac-4153-9b16-"
      "55e86ada0e54}\" "
      "AND System.Devices.Dnssd.ServiceName:=\"%s._tcp\" AND "
      "System.Devices.Dnssd.Domain:=\"local\"";
  static const int SERVICE_ID_HASH_LENGTH = 6;
  static constexpr std::string_view SERVICE_ID_FORMAT =
      "_%02X%02X%02X%02X%02X%02X";

  //
  // Private methods
  //

  // Generates preferred listening port. If cannot bind to this port,
  // NSD will assign a random port for the service.
  // TODO: Windows firewall may break the solution, need to further solution to
  // resolve the potential issue
  uint16 GenerateSocketPort(const std::string& service_id);

  // From mDNS device information, to build NsdServiceInfo.
  // the properties are from DeviceInformation and DeviceInformationUpdate.
  // The API gets IP addresses, service name and text attributes of mDNS
  // from these properties,
  NsdServiceInfo GetNsdServiceInformation(
      IMapView<winrt::hstring, IInspectable> properties);

  // mDNS callbacks for advertising and discovery
  fire_and_forget Listener_ConnectionReceived(
      StreamSocketListener listener,
      StreamSocketListenerConnectionReceivedEventArgs const& args);
  fire_and_forget Watcher_DeviceAdded(DeviceWatcher sender,
                                      DeviceInformation deviceInfo);
  fire_and_forget Watcher_DeviceUpdated(
      DeviceWatcher sender, DeviceInformationUpdate deviceInfoUpdate);
  fire_and_forget Watcher_DeviceRemoved(
      DeviceWatcher sender, DeviceInformationUpdate deviceInfoUpdate);
  static void Advertising_StopCompleted(DWORD Status, PVOID pQueryContext,
                                        PDNS_SERVICE_INSTANCE pInstance);

  // Retrieves IP addresses from local machine
  std::vector<std::string> GetIpAddresses();

  std::string GetServiceIdHash();

  // Manages remote connections
  WifiLanService* GetRemoteWifiLanService(
      std::string endpoint, std::unique_ptr<WifiLanService> wifi_lan_service);
  void RemoveRemoteWifiLanService(std::string endpoint);

  // Basic information of Nsd
  location::nearby::Mutex mutex_{};
  std::string service_id_{};
  // TODO(200421848): NsdServiceInfo should support service type
  std::string service_type_{};
  WifiLanMedium* medium_ = nullptr;
  WifiLanService wifi_lan_service_{};
  absl::flat_hash_map<std::string, std::unique_ptr<WifiLanService>>
      remote_wifi_lan_services_ ABSL_GUARDED_BY(mutex_);

  // NSD Status
  int nsd_status_ = NSD_STATUS_IDLE;

  //
  // Dns-sd related properties
  //

  // Advertising properties
  winrt::event_token listener_event_token_{};
  StreamSocketListener stream_socket_listener_{nullptr};
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

  // callbacks for advertising and discovery
  api::WifiLanMedium::AcceptedConnectionCallback accepted_connection_callback_;
  api::WifiLanMedium::DiscoveredServiceCallback discovered_service_callback_;

  // IP addresses of the computer. mDNS uses them to advertise.
  std::vector<std::string> ip_addresses_{};
};

// Container of operations that can be performed over the WifiLan medium.
class WifiLanMedium : public api::WifiLanMedium {
 public:
  ~WifiLanMedium() override = default;

  // Starts to advertising
  bool StartAdvertising(const std::string& service_id,
                        const NsdServiceInfo& nsd_service_info) override;

  // Stops to advertising
  bool StopAdvertising(const std::string& service_id) override;

  // Starts to discovery
  bool StartDiscovery(const std::string& service_id,
                      DiscoveredServiceCallback callback) override;

  // Returns true once WifiLan discovery for service_id is well and truly
  // stopped; after this returns, there must be no more invocations of the
  // DiscoveredServiceCallback passed in to StartDiscovery() for service_id.
  bool StopDiscovery(const std::string& service_id) override;

  // Returns true once WifiLan socket connection requests to service_id can be
  // accepted.
  bool StartAcceptingConnections(const std::string& service_id,
                                 AcceptedConnectionCallback callback) override;

  // Stops to accept connections
  bool StopAcceptingConnections(const std::string& service_id) override;

  // Connects to a WifiLan service.
  // On success, returns a new WifiLanSocket.
  // On error, returns nullptr.
  std::unique_ptr<api::WifiLanSocket> Connect(
      api::WifiLanService& wifi_lan_service, const std::string& service_id,
      CancellationFlag* cancellation_flag) override;

  // Returns WiFi LAN service from local ip address and port information
  api::WifiLanService* GetRemoteService(const std::string& ip_address,
                                        int port) override;

  // returns advertising service address
  std::pair<std::string, int> GetCredentials(
      const std::string& service_id) override;

  // for internal to clean closed connection.
  void CloseConnection(WifiLanSocket& socket);

 private:
  // Accesses NSD by service id
  WifiLanNsd* GetNsd(std::string service_id, bool create = false);
  bool RemoveNsd(std::string service_id);

  // Gets error message from exception pointer
  std::string GetErrorMessage(std::exception_ptr eptr);

  // Protects the access to NSD and connections
  location::nearby::Mutex mutex_{};

  // Tracks of active advertising or discovery
  absl::flat_hash_map<std::string, std::unique_ptr<WifiLanNsd>>
      service_to_nsd_map_ ABSL_GUARDED_BY(mutex_);

  // Tracks of active connetions
  absl::flat_hash_set<WifiLanSocket*> wifi_lan_sockets_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace windows
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_IMPL_WINDOWS_WIFI_LAN_H_
