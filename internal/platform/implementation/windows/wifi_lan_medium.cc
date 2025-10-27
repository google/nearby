// Copyright 2021 Google LLC
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

#include "internal/platform/implementation/windows/wifi_lan.h"

// Windows headers
#include <windows.h>
#include <winsock2.h>
#include <iphlpapi.h>

// Standard C/C++ headers
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// ABSL headers
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"

// Nearby connections headers
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"

// Nearby connections headers
#include "absl/container/flat_hash_map.h"
#include "internal/flags/nearby_flags.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/cancellation_flag_listener.h"
#include "internal/platform/exception.h"
#include "internal/platform/feature_flags.h"
#include "internal/platform/flags/nearby_platform_feature_flags.h"
#include "internal/platform/implementation/wifi_lan.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Devices.Enumeration.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Foundation.Collections.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Networking.Connectivity.h"
#include "internal/platform/implementation/windows/network_info.h"
#include "internal/platform/implementation/windows/socket_address.h"
#include "internal/platform/implementation/windows/string_utils.h"
#include "internal/platform/implementation/windows/utils.h"
#include "internal/platform/logging.h"
#include "internal/platform/nsd_service_info.h"
#include "internal/platform/runnable.h"

namespace nearby::windows {
namespace {
using ::winrt::fire_and_forget;
using ::winrt::Windows::Devices::Enumeration::DeviceInformation;
using ::winrt::Windows::Devices::Enumeration::DeviceInformationKind;
using ::winrt::Windows::Devices::Enumeration::DeviceInformationUpdate;
using ::winrt::Windows::Devices::Enumeration::DeviceWatcher;
using ::winrt::Windows::Foundation::Collections::IMapView;
using ::winrt::Windows::Networking::Connectivity::NetworkInformation;

// mDNS text attributes
constexpr absl::string_view kDeviceEndpointInfo = "n";
constexpr absl::string_view kDeviceIpv4 = "IPv4";

// mDNS information for advertising and discovery
constexpr absl::string_view kMdnsDeviceSelectorFormat =
    "System.Devices.AepService.ProtocolId:=\"{4526e8c1-8aac-4153-9b16-"
    "55e86ada0e54}\" "
    "AND System.Devices.Dnssd.ServiceName:=\"%s\" AND "
    "System.Devices.Dnssd.Domain:=\"local\"";

constexpr absl::Duration kConnectTimeout = absl::Seconds(1);

bool IsSelfInstance(IMapView<winrt::hstring, IInspectable> properties,
                     absl::string_view self_instance_name) {
  IInspectable inspectable =
      properties.TryLookup(L"System.Devices.Dnssd.InstanceName");
  if (inspectable == nullptr) {
    VLOG(1) << "no service name information in device information.";
    // Unable to find instance name, so treat it as a self instance.
    return true;
  }
  // Don't discover itself
  if (InspectableReader::ReadString(inspectable) == self_instance_name) {
    VLOG(1) << "Don't update WIFI_LAN device for itself.";
    return true;
  }
  return false;
}

bool GetMdnsIpv4Address(const std::string& address_str,
                        NsdServiceInfo& nsd_service_info) {
  SocketAddress ipv4_address(/*dual_stack=*/false);
  if (!SocketAddress::FromString(ipv4_address, address_str)) {
    return false;
  }
  DCHECK_EQ(ipv4_address.address()->sa_family, AF_INET);
  if (ipv4_address.address()->sa_family == AF_INET) {
    std::string ip_address_bytes;
    ip_address_bytes.resize(4);
    const sockaddr_in* ipv4_addr = ipv4_address.ipv4_address();
    std::memcpy(ip_address_bytes.data(), &ipv4_addr->sin_addr.s_addr, 4);
    nsd_service_info.SetIPAddress(ip_address_bytes);
    VLOG(1) << "Found ipv4 address: " <<ipv4_address.ToString();
    return true;
  }
  // Should not reach here.
  return false;
}

bool GetMdnsIpv6Address(const std::string& address_str,
                        IMapView<winrt::hstring, IInspectable> properties,
                        NsdServiceInfo& nsd_service_info) {
  SocketAddress ipv6_address(/*dual_stack=*/true);
  if (!SocketAddress::FromString(ipv6_address, address_str)) {
    return false;
  }
  DCHECK_EQ(ipv6_address.address()->sa_family, AF_INET6);
  if (ipv6_address.address()->sa_family == AF_INET6) {
    if (ipv6_address.IsV6LinkLocal()) {
      // Skip link local addresses if the interface index is not available.
      NET_IFINDEX interface_index = 0;
      IInspectable inspectable =
          properties.TryLookup(L"System.Devices.Dnssd.NetworkAdapterId");
      if (inspectable == nullptr) {
        return false;
      }
      GUID network_adapter_id = InspectableReader::ReadGuid(inspectable);
      NET_LUID luid;
      if (ConvertInterfaceGuidToLuid(&network_adapter_id, &luid) !=
          NO_ERROR) {
        VLOG(1) << "Failed to get interface luid";
        return false;
      }
      if (ConvertInterfaceLuidToIndex(&luid, &interface_index) !=
          NO_ERROR) {
        VLOG(1) << "Failed to get interface index";
        return false;
      }
      VLOG(1) << "network adapter luid: " << luid.Value
              << ", interface index: " << interface_index;
      ipv6_address.SetScopeId(interface_index);
    }
    nsd_service_info.SetIPv6Address(ipv6_address.ToString());
    VLOG(1) << "Found ipv6 address: " <<ipv6_address.ToString();
    return true;
  }
  // Should not reach here.
  return false;
}

// Returns true if a connection can be established to the given address within
// the given timeout.
bool TestConnection(
    const SocketAddress& address, absl::Duration timeout) {
  bool result = false;
  int error = -1;
  int size = sizeof(int);
  timeval tm;
  fd_set set;
  unsigned long non_blocking = 1;  // NOLINT
  VLOG(1) << "Checking connection to: " << address.ToString();
  SOCKET sock = socket(address.dual_stack() ? AF_INET6 : AF_INET, SOCK_STREAM,
                       IPPROTO_TCP);
  if (address.dual_stack()) {
    // On Windows dual stack is not the default.
    // https://learn.microsoft.com/en-us/windows/win32/winsock/dual-stack-sockets#creating-a-dual-stack-socket
    DWORD v6_only = 0;
    if (setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY,
                   reinterpret_cast<const char*>(&v6_only),
                   sizeof(v6_only)) == SOCKET_ERROR) {
      LOG(WARNING) << "Failed to set IPV6_V6ONLY with error "
                   << WSAGetLastError();
    }
  }
  ioctlsocket(sock, /*cmd=*/FIONBIO, /*argp=*/&non_blocking);
  if (connect(sock, address.address(), sizeof(sockaddr_storage)) ==
      SOCKET_ERROR) {
    tm.tv_sec = timeout / absl::Seconds(1);
    tm.tv_usec = 0;
    FD_ZERO(&set);
    FD_SET(sock, &set);

    if (select(sock + 1, nullptr, &set, nullptr, &tm) > 0) {
      getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&error,
                 /*(socklen_t *)*/ &size);
      result = error == 0;
    } else {
      result = false;
    }
  } else {
    result = true;
  }

  non_blocking = 0;
  ioctlsocket(sock, /*cmd=*/FIONBIO, /*argp=*/&non_blocking);

  if (result) {
    closesocket(sock);
  }

  return result;
}

}  // namespace

bool WifiLanMedium::IsNetworkConnected() const {
  // connection_profile will be null when there's no network adapter or
  // connection to a network. For example, WiFi isn't connected to an AP/hotspot
  // and ethernet isn't connected to a router/hub/switch.
  auto connection_profile = NetworkInformation::GetInternetConnectionProfile();
  return connection_profile != nullptr;
}

bool WifiLanMedium::StartAdvertising(const NsdServiceInfo& nsd_service_info) {
  if (nsd_service_info.GetTxtRecord(std::string(kDeviceEndpointInfo)).empty()) {
    LOG(ERROR) << "Cannot start advertising without endpoint info.";
    return false;
  }
  if (nsd_service_info.GetServiceName().empty()) {
    LOG(ERROR) << "Cannot start advertising without service name.";
    return false;
  }
  if (IsAdvertising()) {
    LOG(WARNING) << "Cannot start advertising again when it is running.";
    return false;
  }

  if (!port_to_server_socket_map_.contains(nsd_service_info.GetPort())) {
    LOG(WARNING) << "Cannot start advertising without a listening socket.";
    return false;
  }
  service_name_ = nsd_service_info.GetServiceName();
  if (wifi_lan_mdns_.StartMdnsService(
          service_name_, nsd_service_info.GetServiceType(),
          nsd_service_info.GetPort(), nsd_service_info.GetTxtRecords())) {
    LOG(INFO) << "started mDNS advertising for: " << service_name_
              << " on port " << nsd_service_info.GetPort();
    medium_status_ |= kMediumStatusAdvertising;
    return true;
  }
  LOG(ERROR) << "failed to start mDNS advertising for: " << service_name_;
  return false;
}

bool WifiLanMedium::StopAdvertising(const NsdServiceInfo& nsd_service_info) {
  // Need to use Win32 API to deregister the Dnssd instance
  if (!IsAdvertising()) {
    LOG(WARNING)
        << "Cannot stop advertising because no advertising is running.";
    return false;
  }

  // The service may be running under WinRT when the flag is enabled.
  dnssd_service_instance_ = nullptr;

  bool result = wifi_lan_mdns_.StopMdnsService();

  if (result) {
    medium_status_ &= (~kMediumStatusAdvertising);
    service_name_.clear();
    return true;
  }

  LOG(ERROR) << "failed to stop mDNS advertising.";
  medium_status_ &= (~kMediumStatusAdvertising);
  return false;
}

// Returns true once the WifiLan discovery has been initiated.
bool WifiLanMedium::StartDiscovery(const std::string& service_type,
                                   DiscoveredServiceCallback callback) {
  if (IsDiscovering()) {
    LOG(WARNING) << "discovery already running for service type ="
                 << service_type;
    return false;
  }

  // In WifiLan::StartDiscovery(), service_type is appended with "._tcp." for
  // ios and android platform. For windows, this has to be removed because
  // "._tcp" will be appended in following "selector"
  std::string service_type_trim = service_type;
  if (service_type.size() > 5 &&
      (service_type.rfind("_tcp.") == service_type.size() - 5)) {
    service_type_trim.resize(service_type_trim.size() - 1);
  }

  std::string selector =
      absl::StrFormat(kMdnsDeviceSelectorFormat.data(), service_type_trim);

  std::vector<winrt::hstring> requestedProperties{
      L"System.Devices.IpAddress",
      L"System.Devices.Dnssd.NetworkAdapterId",
      L"System.Devices.Dnssd.HostName",
      L"System.Devices.Dnssd.InstanceName",
      L"System.Devices.Dnssd.PortNumber",
      L"System.Devices.Dnssd.ServiceName",
      L"System.Devices.Dnssd.TextAttributes"};

  device_watcher_ = DeviceInformation::CreateWatcher(
      string_utils::StringToWideString(selector), requestedProperties,
      DeviceInformationKind::AssociationEndpointService);

  device_watcher_added_event_token =
      device_watcher_.Added({this, &WifiLanMedium::Watcher_DeviceAdded});
  device_watcher_updated_event_token =
      device_watcher_.Updated({this, &WifiLanMedium::Watcher_DeviceUpdated});
  device_watcher_removed_event_token =
      device_watcher_.Removed({this, &WifiLanMedium::Watcher_DeviceRemoved});

  // clear discovered mDNS instances.
  ClearDiscoveredServices();

  device_watcher_.Start();
  discovered_service_callback_ = std::move(callback);
  medium_status_ |= kMediumStatusDiscovering;

  LOG(INFO) << "started to discovery.";

  return true;
}

// Returns true once WifiLan discovery for service_id is well and truly
// stopped; after this returns, there must be no more invocations of the
// DiscoveredServiceCallback passed in to StartDiscovery() for service_id.
bool WifiLanMedium::StopDiscovery(const std::string& service_type) {
  if (!IsDiscovering()) {
    LOG(WARNING) << "no discovering service to stop.";
    return false;
  }
  device_watcher_.Stop();
  device_watcher_.Added(device_watcher_added_event_token);
  device_watcher_.Updated(device_watcher_updated_event_token);
  device_watcher_.Removed(device_watcher_removed_event_token);
  medium_status_ &= (~kMediumStatusDiscovering);
  device_watcher_ = nullptr;
  return true;
}

std::unique_ptr<api::WifiLanSocket> WifiLanMedium::ConnectToService(
    const NsdServiceInfo& remote_service_info,
    CancellationFlag* cancellation_flag) {
  LOG(INFO) << "connect to service by NSD service info: "
            << remote_service_info.GetServiceName() << "."
            << remote_service_info.GetServiceType();

  std::string ipv4_address = remote_service_info.GetIPAddress();
  if (!ipv4_address.empty()) {
    std::unique_ptr<api::WifiLanSocket> socket = ConnectToService(
        ipv4_address, remote_service_info.GetPort(), cancellation_flag);
    if (socket != nullptr) {
      return socket;
    }
    LOG(WARNING) << "Failed to connect to service by IPv4 address";
  }
  if (!NearbyFlags::GetInstance().GetBoolFlag(
          platform::config_package_nearby::nearby_platform_feature::
              kEnableMdnsIpv6)) {
    return nullptr;
  }
  std::string ipv6_address = remote_service_info.GetIPv6Address();
  if (ipv6_address.empty()) {
    return nullptr;
  }
  SocketAddress server_address(/*dual_stack=*/true);
  if (!server_address.FromString(server_address, ipv6_address,
                                 remote_service_info.GetPort())) {
    return nullptr;
  }
  std::unique_ptr<api::WifiLanSocket> socket =
      ConnectToSocket(server_address, cancellation_flag);
  if (socket != nullptr) {
    return socket;
  }
  VLOG(1) << "Failed to connect to service by IPv6 address: "
          << ipv6_address;
  return nullptr;
}

std::unique_ptr<api::WifiLanSocket> WifiLanMedium::ConnectToService(
    const std::string& ip_address, int port,
    CancellationFlag* cancellation_flag) {
  LOG(INFO) << "ConnectToService is called.";
  bool dual_stack = NearbyFlags::GetInstance().GetBoolFlag(
      platform::config_package_nearby::nearby_platform_feature::
          kEnableIpv6DualStack);
  SocketAddress server_address(dual_stack);
  if (!server_address.FromBytes(server_address, ip_address, port)) {
    LOG(ERROR) << "no valid service address and port to connect.";
    return nullptr;
  }
  return ConnectToSocket(server_address, cancellation_flag);
}

std::unique_ptr<api::WifiLanSocket> WifiLanMedium::ConnectToSocket(
    const SocketAddress& address,
    CancellationFlag* cancellation_flag) {
  if (cancellation_flag != nullptr && cancellation_flag->Cancelled()) {
    LOG(INFO) << "connect to service has been cancelled.";
    return nullptr;
  }
  VLOG(1) << "ConnectToSocket: " << address.ToString();
  std::unique_ptr<CancellationFlagListener> connection_cancellation_listener =
      nullptr;

  auto wifi_lan_socket = std::make_unique<WifiLanSocket>();

  // setup cancel listener
  if (cancellation_flag != nullptr) {
    connection_cancellation_listener =
        std::make_unique<nearby::CancellationFlagListener>(
            cancellation_flag, [socket = wifi_lan_socket.get()]() {
              LOG(WARNING) << "connect is closed due to it is cancelled.";
              socket->Close();
            });
  }
  bool result = wifi_lan_socket->Connect(address);
  if (!result) {
    LOG(ERROR) << "failed to connect to service.";
    return nullptr;
  }

  LOG(INFO) << "connected to remote service.";

  return wifi_lan_socket;
}

std::unique_ptr<api::WifiLanServerSocket> WifiLanMedium::ListenForService(
    int port) {
  // check current status
  const auto& it = port_to_server_socket_map_.find(port);
  if (it != port_to_server_socket_map_.end()) {
    LOG(WARNING) << "accepting connections already started on port "
                 << it->second->GetPort();
    return nullptr;
  }
  std::unique_ptr<WifiLanServerSocket> server_socket =
      std::make_unique<WifiLanServerSocket>(port);
  WifiLanServerSocket* server_socket_ptr = server_socket.get();

  bool dual_stack = NearbyFlags::GetInstance().GetBoolFlag(
      platform::config_package_nearby::nearby_platform_feature::
          kEnableIpv6DualStack);
  if (server_socket->Listen(dual_stack)) {
    int port = server_socket_ptr->GetPort();
    LOG(INFO) << "started to listen serive on port: " << port;
    port_to_server_socket_map_.insert({port, server_socket_ptr});

    server_socket->SetCloseNotifier([this, server_socket_ptr, port]() {
      if (port_to_server_socket_map_.contains(port) &&
          port_to_server_socket_map_[port] == server_socket_ptr) {
        LOG(INFO) << "Server socket was closed on port " << port;
        port_to_server_socket_map_[port] = nullptr;
        port_to_server_socket_map_.erase(port);
      } else {
        LOG(INFO) << " The closing port doesn't match with the record "
                     "in port_to_server_socket_map_ map for port: "
                  << port;
      }
    });

    return server_socket;
  }

  LOG(ERROR) << "Failed to listen service on port " << port;

  return nullptr;
}

ExceptionOr<NsdServiceInfo> WifiLanMedium::GetNsdServiceInformation(
    IMapView<winrt::hstring, IInspectable> properties, bool is_device_found) {
  NsdServiceInfo nsd_service_info{};

  // Read service name information
  IInspectable inspectable =
      properties.TryLookup(L"System.Devices.Dnssd.InstanceName");
  if (inspectable == nullptr) {
    VLOG(1) << "no service name information in device information.";
    return Exception{Exception::kFailed};
  }
  nsd_service_info.SetServiceName(InspectableReader::ReadString(inspectable));

  // Read service type information
  inspectable = properties.TryLookup(L"System.Devices.Dnssd.ServiceName");
  if (inspectable == nullptr) {
    VLOG(1) << "no service type information in device information.";
    return Exception{Exception::kFailed};
  }

  // In WifiLan::StartDiscovery(), service_type is appended with "._tcp." for
  // ios and android platform. For windows, we only have "._tcp" as appendix.
  // Here "." is added back to match the upper layer service_type, because
  // service_type is used to get the corresponding call back function.
  nsd_service_info.SetServiceType(
      (InspectableReader::ReadString(inspectable)).append("."));

  // Read text records
  inspectable = properties.TryLookup(L"System.Devices.Dnssd.TextAttributes");
  if (inspectable == nullptr) {
    VLOG(1) << "No text attributes information in device information.";
    return Exception{Exception::kFailed};
  }

  auto text_attributes = InspectableReader::ReadStringArray(inspectable);
  for (auto text_attribute : text_attributes) {
    // text attribute in format key=value
    int pos = text_attribute.find("=");
    if (pos <= 0 || pos == text_attribute.size() - 1) {
      VLOG(1) << "found invalid text attribute " << text_attribute;
      continue;
    }

    std::string key = text_attribute.substr(0, pos);
    std::string value = text_attribute.substr(pos + 1);
    nsd_service_info.SetTxtRecord(key, value);
  }
  // mDNS record must have device endpoint info.
  if (nsd_service_info.GetTxtRecord(kDeviceEndpointInfo.data()).empty()) {
    LOG(ERROR) << "No device endpoint info found.";
    return Exception{Exception::kFailed};
  }
  if (!is_device_found) {
    return ExceptionOr<NsdServiceInfo>(nsd_service_info);
  }

  // Read IP port
  inspectable = properties.TryLookup(L"System.Devices.Dnssd.PortNumber");
  if (inspectable == nullptr) {
    VLOG(1) << "no IP port property in device information.";
    return Exception{Exception::kFailed};
  }

  int port = InspectableReader::ReadUint16(inspectable);
  nsd_service_info.SetPort(port);

  // Read IP Address information
  // Use the mDNS resolved IP addresses first.  If not available, use the IP
  // addresses from TXT record.
  std::vector<std::string> ip_address_candidates;

  inspectable = properties.TryLookup(L"System.Devices.IPAddress");
  if (inspectable != nullptr) {
    ip_address_candidates = InspectableReader::ReadStringArray(inspectable);
  }
  std::string ipv4_address =
      nsd_service_info.GetTxtRecord(std::string(kDeviceIpv4));
  if (!ipv4_address.empty()) {
    ip_address_candidates.push_back(ipv4_address);
  }

  bool has_ipv4_address = false;
  bool has_ipv6_address = false;
  for (std::string& address : ip_address_candidates) {
    if (has_ipv4_address && has_ipv6_address) {
      // Windows only provide 1 of each of IPv4 and IPv6 addresses.
      break;
    }
    if (!has_ipv4_address) {
      if (GetMdnsIpv4Address(address, nsd_service_info)) {
        has_ipv4_address = true;
        continue;
      }
    }
    if (!has_ipv6_address) {
      if (GetMdnsIpv6Address(address, properties, nsd_service_info)) {
        has_ipv6_address = true;
        continue;
      }
    }
  }
  if (!has_ipv4_address && !has_ipv6_address) {
    VLOG(1) << "no IP addresses in mDNS record.";
    return Exception{Exception::kFailed};
  }
  return ExceptionOr<NsdServiceInfo>(nsd_service_info);
}

fire_and_forget WifiLanMedium::Watcher_DeviceAdded(
    DeviceWatcher sender, DeviceInformation deviceInfo) {
  VLOG(1) << "WifiLanMedium::Watcher_DeviceAdded";
  if (IsSelfInstance(deviceInfo.Properties(), service_name_)) {
    return fire_and_forget{};
  }
  // need to read IP address and port information from deviceInfo
  ExceptionOr<NsdServiceInfo> nsd_service_info_except =
      GetNsdServiceInformation(deviceInfo.Properties(),
                               /*is_device_found*/ true);

  if (!nsd_service_info_except.ok()) {
    VLOG(1) << "NSD information is incompleted or has error!  Don't add "
               "WIFI_LAN device.";
    return fire_and_forget{};
  }

  NsdServiceInfo nsd_service_info = nsd_service_info_except.GetResult();
  LOG(INFO) << "device found for service name "
            << nsd_service_info.GetServiceName()
            << " on port " << nsd_service_info.GetPort();

  if (!IsConnectableIpAddress(nsd_service_info, kConnectTimeout)) {
    VLOG(1) << "mDNS service " << nsd_service_info.GetServiceName()
            << " is not reachable.";
    return fire_and_forget{};
  }

  UpdateDiscoveredService(winrt::to_string(deviceInfo.Id()), nsd_service_info);
  discovered_service_callback_.service_discovered_cb(nsd_service_info);

  return fire_and_forget();
}

fire_and_forget WifiLanMedium::Watcher_DeviceUpdated(
    DeviceWatcher sender, DeviceInformationUpdate deviceInfoUpdate) {
  VLOG(1) << "WifiLanMedium::Watcher_DeviceUpdated";
  if (IsSelfInstance(deviceInfoUpdate.Properties(), service_name_)) {
    return fire_and_forget{};
  }
  ExceptionOr<NsdServiceInfo> nsd_service_info_except =
      GetNsdServiceInformation(deviceInfoUpdate.Properties(),
                               /*is_device_found*/ true);

  if (!nsd_service_info_except.ok()) {
    VLOG(1) << "NSD information is incompleted or has error!";
    return fire_and_forget{};
  }

  NsdServiceInfo nsd_service_info = nsd_service_info_except.GetResult();

  // check having any changes
  std::optional<NsdServiceInfo> last_nsd_service_info =
      GetDiscoveredService(winrt::to_string(deviceInfoUpdate.Id()));
  if (!last_nsd_service_info.has_value()) {
    LOG(INFO) << "device updated for service name "
              << nsd_service_info.GetServiceName()
              << " on port " << nsd_service_info.GetPort();
    if (IsConnectableIpAddress(nsd_service_info, kConnectTimeout)) {
      // If the device is not in the discovered service list, but it is
      // connectable during update, we add it to the discovered service list.
      UpdateDiscoveredService(winrt::to_string(deviceInfoUpdate.Id()),
                              nsd_service_info);
      discovered_service_callback_.service_discovered_cb(nsd_service_info);
      return fire_and_forget{};
    }

    VLOG(1) << "mDNS service " << nsd_service_info.GetServiceName()
            << " is not reachable.";
    return fire_and_forget{};
  }

  if ((last_nsd_service_info->GetTxtRecord(std::string(kDeviceEndpointInfo)) ==
       nsd_service_info.GetTxtRecord(std::string(kDeviceEndpointInfo))) &&
      (last_nsd_service_info->GetServiceName() ==
       nsd_service_info.GetServiceName()) &&
      (last_nsd_service_info->GetIPAddress() ==
       nsd_service_info.GetIPAddress()) &&
      (last_nsd_service_info->GetIPv6Address() ==
       nsd_service_info.GetIPv6Address()) &&
      (last_nsd_service_info->GetPort() == nsd_service_info.GetPort())) {
    VLOG(1) << "Don't update WIFI_LAN device since there is no change.";
    return fire_and_forget{};
  }

  LOG(INFO)
      << "Device is changed from (service name:"
      << last_nsd_service_info->GetServiceName() << ", endpoint info:"
      << last_nsd_service_info->GetTxtRecord(std::string(kDeviceEndpointInfo))
      << ", port: " << last_nsd_service_info->GetPort()
      << ") to (service name:" << nsd_service_info.GetServiceName() << ", "
      << nsd_service_info.GetTxtRecord(std::string(kDeviceEndpointInfo))
      << ", port: " << nsd_service_info.GetPort() << ").";

  // Report device lost first.
  discovered_service_callback_.service_lost_cb(*last_nsd_service_info);
  UpdateDiscoveredService(winrt::to_string(deviceInfoUpdate.Id()),
                          nsd_service_info);

  // Report the updated device discovered.
  discovered_service_callback_.service_discovered_cb(nsd_service_info);
  return fire_and_forget();
}

fire_and_forget WifiLanMedium::Watcher_DeviceRemoved(
    DeviceWatcher sender, DeviceInformationUpdate deviceInfoUpdate) {
  VLOG(1) << "WifiLanMedium::Watcher_DeviceRemoved";
  if (IsSelfInstance(deviceInfoUpdate.Properties(), service_name_)) {
    return fire_and_forget{};
  }
  // need to read IP address and port information from deviceInfo
  ExceptionOr<NsdServiceInfo> nsd_service_info_except =
      GetNsdServiceInformation(deviceInfoUpdate.Properties(),
                               /*is_device_found*/ false);

  if (!nsd_service_info_except.ok()) {
    VLOG(1) << "NSD information is incompleted or has error! Ignore";
    return fire_and_forget{};
  }

  NsdServiceInfo nsd_service_info = nsd_service_info_except.GetResult();
  LOG(INFO) << "device removed for service name "
            << nsd_service_info.GetServiceName();
  RemoveDiscoveredService(winrt::to_string(deviceInfoUpdate.Id()));
  discovered_service_callback_.service_lost_cb(nsd_service_info);

  return fire_and_forget();
}

void WifiLanMedium::ClearDiscoveredServices() {
  absl::MutexLock lock(mutex_);
  discovered_services_map_.clear();
}

std::optional<NsdServiceInfo> WifiLanMedium::GetDiscoveredService(
    absl::string_view id) {
  absl::MutexLock lock(mutex_);
  auto it = discovered_services_map_.find(id);
  if (it == discovered_services_map_.end()) {
    return std::nullopt;
  }

  return it->second;
}

void WifiLanMedium::UpdateDiscoveredService(
    absl::string_view id, const NsdServiceInfo& nsd_service_info) {
  absl::MutexLock lock(mutex_);
  discovered_services_map_[id] = nsd_service_info;
}

void WifiLanMedium::RemoveDiscoveredService(absl::string_view id) {
  absl::MutexLock lock(mutex_);
  auto it = discovered_services_map_.find(id);
  if (it != discovered_services_map_.end()) {
    discovered_services_map_.erase(it);
  }
}

bool WifiLanMedium::IsConnectableIpAddress(NsdServiceInfo& nsd_service_info,
                                           absl::Duration timeout) {
  std::string ipv4_address = nsd_service_info.GetIPAddress();
  if (!ipv4_address.empty()) {
    bool dual_stack = NearbyFlags::GetInstance().GetBoolFlag(
        platform::config_package_nearby::nearby_platform_feature::
            kEnableIpv6DualStack);
    SocketAddress service_address(dual_stack);
    if (SocketAddress::FromBytes(service_address, ipv4_address,
                                 nsd_service_info.GetPort())) {
      if (TestConnection(service_address, timeout)) {
        return true;
      }
      VLOG(1) << "Failed to connect to IPv4 address: "
              << service_address.ToString();
    }
  }

  if (!NearbyFlags::GetInstance().GetBoolFlag(
    platform::config_package_nearby::nearby_platform_feature::
        kEnableMdnsIpv6)) {
    return false;
  }
  std::string ipv6_address = nsd_service_info.GetIPv6Address();
  if (ipv6_address.empty()) {
    return false;
  }
  SocketAddress server_address(/*dual_stack=*/true);
  if (!server_address.FromString(server_address, ipv6_address,
                                 nsd_service_info.GetPort())) {
    return false;
  }
  if (TestConnection(server_address, timeout)) {
    return true;
  }
  VLOG(1) << "Failed to connect to IPv6 address: " << ipv6_address;
  return false;
}

std::vector<std::string> WifiLanMedium::GetUpgradeAddressCandidates(
    const api::WifiLanServerSocket& server_socket) {
  const NetworkInfo& network_info = NetworkInfo::GetNetworkInfo();
  std::vector<std::string> ip_addresses;
  std::vector<std::string> ipv4_addresses;
  for (const auto& net_interface : network_info.GetInterfaces()) {
    // Only use wifi and ethernet interfaces for upgrade.
    if (net_interface.type != InterfaceType::kWifi &&
        net_interface.type != InterfaceType::kEthernet) {
      continue;
    }
    for (const auto& ipv6_address : net_interface.ipv6_addresses) {
      SocketAddress address(ipv6_address);
      // Link local addresses cannot be used for upgrade since we can't tell
      // which interface on the remote device the address is valid.
      if (address.IsV6LinkLocal()) {
        continue;
      }
      ip_addresses.push_back(
          std::string(reinterpret_cast<const char*>(
                          &address.ipv6_address()->sin6_addr.u.Byte[0]),
                      16));
    }
    for (const auto& ipv4address : net_interface.ipv4_addresses) {
      auto address = reinterpret_cast<const sockaddr_in*>(&ipv4address);
      ipv4_addresses.push_back(std::string(
          reinterpret_cast<const char*>(&address->sin_addr.S_un.S_un_b.s_b1),
          4));
    }
  }
  // Append v4 addresses to the end of the list.
  ip_addresses.insert(ip_addresses.end(), ipv4_addresses.begin(),
                      ipv4_addresses.end());
  return ip_addresses;
}

}  // namespace nearby::windows
