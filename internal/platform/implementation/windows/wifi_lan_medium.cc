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
// clang-format off
#include <winsock2.h>
#include <iphlpapi.h>
// clang-format on

// Standard C/C++ headers
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "internal/flags/nearby_flags.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/cancellation_flag_listener.h"
#include "internal/platform/exception.h"
#include "internal/platform/feature_flags.h"
#include "internal/platform/flags/nearby_platform_feature_flags.h"
#include "internal/platform/service_address.h"
#include "internal/platform/implementation/upgrade_address_info.h"
#include "internal/platform/implementation/wifi_lan.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Devices.Enumeration.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Foundation.Collections.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Networking.Connectivity.h"
#include "internal/platform/implementation/windows/nearby_client_socket.h"
#include "internal/platform/implementation/windows/network_info.h"
#include "internal/platform/implementation/windows/registry.h"
#include "internal/platform/implementation/windows/socket_address.h"
#include "internal/platform/implementation/windows/string_utils.h"
#include "internal/platform/implementation/windows/utils.h"
#include "internal/platform/logging.h"
#include "internal/platform/nsd_service_info.h"

namespace nearby::windows {
namespace {
using ::nearby::platform::windows::Registry;
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

constexpr absl::string_view kDisableMdnsAdvertisingRegistryValue =
    "disable_mdns_advertising";

// From metrics, P90 wifi connection latency is just under 600ms.
// Set to 700ms to be slightly more generous than the P90.
constexpr absl::Duration kConnectTimeout = absl::Milliseconds(700);

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
  SocketAddress ipv4_address;
  if (!SocketAddress::FromString(ipv4_address, address_str)) {
    return false;
  }
  if (ipv4_address.family() == AF_INET) {
    std::string ip_address_bytes;
    ip_address_bytes.resize(4);
    const sockaddr_in* ipv4_addr = ipv4_address.ipv4_address();
    std::memcpy(ip_address_bytes.data(), &ipv4_addr->sin_addr.s_addr, 4);
    nsd_service_info.SetIPAddress(ip_address_bytes);
    VLOG(1) << "Found ipv4 address: " << ipv4_address.ToString();
    return true;
  }
  // address_str is not a valid ipv4 address.
  return false;
}

bool GetMdnsIpv6Address(const std::string& address_str,
                        IMapView<winrt::hstring, IInspectable> properties,
                        NsdServiceInfo& nsd_service_info) {
  SocketAddress ipv6_address;
  if (!SocketAddress::FromString(ipv6_address, address_str)) {
    return false;
  }
  if (ipv6_address.family() == AF_INET6) {
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
      if (ConvertInterfaceGuidToLuid(&network_adapter_id, &luid) != NO_ERROR) {
        VLOG(1) << "Failed to get interface luid";
        return false;
      }
      if (ConvertInterfaceLuidToIndex(&luid, &interface_index) != NO_ERROR) {
        VLOG(1) << "Failed to get interface index";
        return false;
      }
      VLOG(1) << "network adapter luid: " << luid.Value
              << ", interface index: " << interface_index;
      ipv6_address.SetScopeId(interface_index);
    }
    nsd_service_info.SetIPv6Address(ipv6_address.ToString());
    VLOG(1) << "Found ipv6 address: " << ipv6_address.ToString();
    return true;
  }
  // Should not reach here.
  return false;
}

// Returns true if a connection can be established to the given address within
// the given timeout.
bool TestConnection(const SocketAddress& address, absl::Duration timeout) {
  VLOG(1) << "Checking connection to: " << address.ToString();
  NearbyClientSocket client_socket;
  if (!client_socket.Connect(address, timeout)) {
    return false;
  }
  return true;
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
  if (Registry::ReadDword(Registry::Hive::kCurrentUser,
                          Registry::Key::kNearbyShareFlags,
                          std::string(kDisableMdnsAdvertisingRegistryValue))
          .value_or(0) != 0) {
    LOG(INFO) << "MDNS advertising is disabled by registry.";
    medium_status_ |= kMediumStatusAdvertising;
    return true;
  }
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

  // Note: Do not clear the service_name_ when stopping advertising.
  // It's needed for discovery to filter out our own mDNS records.
  // TODO: b/456199356 - Look into a more robust way to handle self discovery.
  if (result) {
    medium_status_ &= (~kMediumStatusAdvertising);
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
    SocketAddress v4_server_address;
    if (SocketAddress::FromBytes(v4_server_address, ipv4_address,
                                 remote_service_info.GetPort())) {
      std::unique_ptr<api::WifiLanSocket> socket = ConnectToSocket(
          v4_server_address, cancellation_flag, kConnectTimeout);
      if (socket != nullptr) {
        return socket;
      }
    }
  }
  std::string ipv6_address = remote_service_info.GetIPv6Address();
  if (ipv6_address.empty()) {
    return nullptr;
  }
  SocketAddress v6_server_address;
  if (!SocketAddress::FromString(v6_server_address, ipv6_address,
                                 remote_service_info.GetPort())) {
    return nullptr;
  }
  std::unique_ptr<api::WifiLanSocket> socket =
      ConnectToSocket(v6_server_address, cancellation_flag, kConnectTimeout);
  if (socket != nullptr) {
    return socket;
  }
  return nullptr;
}

std::unique_ptr<api::WifiLanSocket> WifiLanMedium::ConnectToService(
    const ServiceAddress& service_address,
    CancellationFlag* cancellation_flag) {
  LOG(INFO) << "ConnectToService is called.";
  SocketAddress server_address;
  if (!server_address.FromServiceAddress(server_address, service_address)) {
    LOG(ERROR) << "no valid service address and port to connect.";
    return nullptr;
  }
  return ConnectToSocket(server_address, cancellation_flag, kConnectTimeout);
}

std::unique_ptr<api::WifiLanSocket> WifiLanMedium::ConnectToSocket(
    const SocketAddress& address, CancellationFlag* cancellation_flag,
    absl::Duration timeout) {
  if (cancellation_flag != nullptr && cancellation_flag->Cancelled()) {
    LOG(INFO) << "connect to service has been cancelled.";
    return nullptr;
  }
  VLOG(1) << "ConnectToSocket: " << address.ToString();
  std::unique_ptr<CancellationFlagListener> connection_cancellation_listener;

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
  bool result = wifi_lan_socket->Connect(address, timeout);
  if (!result) {
    LOG(ERROR) << "failed to connect to service using "
               << (address.family() == AF_INET6 ? "IPv6" : "IPv4");
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
      std::make_unique<WifiLanServerSocket>();
  WifiLanServerSocket* server_socket_ptr = server_socket.get();

  if (server_socket->Listen(port)) {
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
            << nsd_service_info.GetServiceName() << " on port "
            << nsd_service_info.GetPort();

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
              << nsd_service_info.GetServiceName() << " on port "
              << nsd_service_info.GetPort();
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

  LOG(INFO) << "Device is changed from (service name:"
            << last_nsd_service_info->GetServiceName() << ", endpoint info:"
            << last_nsd_service_info->GetTxtRecord(
                   std::string(kDeviceEndpointInfo))
            << ", port: " << last_nsd_service_info->GetPort()
            << ") to (service name:" << nsd_service_info.GetServiceName()
            << ", "
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
    SocketAddress service_address;
    if (SocketAddress::FromBytes(service_address, ipv4_address,
                                 nsd_service_info.GetPort())) {
      if (TestConnection(service_address, timeout)) {
        return true;
      }
      VLOG(1) << "Failed to connect to IPv4 address: "
              << service_address.ToString();
    }
  }
  std::string ipv6_address = nsd_service_info.GetIPv6Address();
  if (ipv6_address.empty()) {
    return false;
  }
  SocketAddress server_address;
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

api::UpgradeAddressInfo WifiLanMedium::GetUpgradeAddressCandidates(
    const api::WifiLanServerSocket& server_socket) {
  const NetworkInfo& network_info = NetworkInfo::GetNetworkInfo();
  uint16_t port = server_socket.GetPort();
  api::UpgradeAddressInfo result;
  std::vector<ServiceAddress> ipv4_addresses;
  for (const NetworkInfo::InterfaceInfo& net_interface :
       network_info.GetInterfaces()) {
    bool has_ipv6_address = false;
    bool has_ipv4_address = false;
    // Only use wifi and ethernet interfaces for upgrade.
    if (net_interface.type != InterfaceType::kWifi &&
        net_interface.type != InterfaceType::kEthernet) {
      continue;
    }
    for (const SocketAddress& address : net_interface.ipv6_addresses) {
      // Link local addresses cannot be used for upgrade since we can't tell
      // which interface on the remote device the address is valid.
      if (address.IsV6LinkLocal()) {
        continue;
      }
      result.address_candidates.push_back(address.ToServiceAddress(port));
      has_ipv6_address = true;
    }
    for (const SocketAddress& v4_address : net_interface.ipv4_addresses) {
      // Link local addresses cannot be used for upgrade since we can't tell
      // which interface on the remote device the address is valid.
      if (v4_address.IsV4LinkLocal()) {
        continue;
      }
      ipv4_addresses.push_back(v4_address.ToServiceAddress(port));
      has_ipv4_address = true;
    }
    if (has_ipv6_address || has_ipv4_address) {
      result.num_interfaces++;
      if (has_ipv6_address && !has_ipv4_address) {
        result.num_ipv6_only_interfaces++;
      }
    }
  }
  // Append v4 addresses to the end of the list.
  result.address_candidates.insert(result.address_candidates.end(),
                                    ipv4_addresses.begin(),
                                    ipv4_addresses.end());
  return result;
}

}  // namespace nearby::windows
