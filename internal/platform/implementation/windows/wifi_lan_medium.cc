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
#include <winsock.h>

// Standard C/C++ headers
#include <codecvt>
#include <cstdint>
#include <locale>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// ABSL headers
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"

// Nearby connections headers
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"

// Nearby connections headers
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/cancellation_flag_listener.h"
#include "internal/platform/exception.h"
#include "internal/platform/feature_flags.h"
#include "internal/platform/implementation/windows/string_utils.h"
#include "internal/platform/implementation/windows/utils.h"
#include "internal/platform/logging.h"
#include "internal/platform/nsd_service_info.h"
#include "internal/platform/runnable.h"

namespace nearby {
namespace windows {
namespace {
// mDNS text attributes
constexpr absl::string_view kDeviceEndpointInfo = "n";
constexpr absl::string_view kDeviceIpv4 = "IPv4";

// mDNS information for advertising and discovery
constexpr std::wstring_view kMdnsHostName = L"Windows.local";
constexpr absl::string_view kMdnsInstanceNameFormat = "%s.%slocal";
constexpr absl::string_view kMdnsDeviceSelectorFormat =
    "System.Devices.AepService.ProtocolId:=\"{4526e8c1-8aac-4153-9b16-"
    "55e86ada0e54}\" "
    "AND System.Devices.Dnssd.ServiceName:=\"%s\" AND "
    "System.Devices.Dnssd.Domain:=\"local\"";

constexpr absl::Duration kConnectTimeout = absl::Seconds(2);
constexpr absl::Duration kConnectServiceTimeout = absl::Seconds(3);

}  // namespace

bool WifiLanMedium::IsNetworkConnected() const {
  // connection_profile will be null when there's no network adapter or
  // connection to a network. For example, WiFi isn't connected to an AP/hotspot
  // and ethernet isn't connected to a router/hub/switch.
  auto connection_profile = NetworkInformation::GetInternetConnectionProfile();
  return connection_profile != nullptr;
}

bool WifiLanMedium::StartAdvertising(const NsdServiceInfo& nsd_service_info) {
  bool socket_found = false;
  WifiLanServerSocket* server_socket_ptr = nullptr;
  for (const auto& server_socket : port_to_server_socket_map_) {
    if ((server_socket.second->GetIPAddress() ==
         nsd_service_info.GetIPAddress()) &&
        (server_socket.second->GetPort() == nsd_service_info.GetPort())) {
      LOG(INFO) << "Found the server socket." << " IP: "
                << ipaddr_4bytes_to_dotdecimal_string(
                       nsd_service_info.GetIPAddress())
                << "; port: " << nsd_service_info.GetPort();
      server_socket_ptr = server_socket.second;
      socket_found = true;
      break;
    }
  }
  if (!socket_found) {
    LOG(WARNING) << "cannot start advertising without accepting connetions.";
    return false;
  }

  if (IsAdvertising()) {
    LOG(WARNING) << "cannot start advertising again when it is running.";
    return false;
  }

  if (nsd_service_info.GetTxtRecord(kDeviceEndpointInfo.data()).empty()) {
    LOG(ERROR) << "cannot start advertising without endpoint info.";
    return false;
  }

  if (nsd_service_info.GetServiceName().empty()) {
    LOG(ERROR) << "cannot start advertising without service name.";
    return false;
  }

  service_name_ = nsd_service_info.GetServiceName();

  std::string instance_name =
      absl::StrFormat(kMdnsInstanceNameFormat.data(), service_name_,
                      nsd_service_info.GetServiceType());

  LOG(INFO) << "mDNS instance name is " << instance_name;

  dnssd_service_instance_ = DnssdServiceInstance{
      string_utils::StringToWideString(instance_name),
      nullptr,  // let windows use default computer's local name
      (uint16)nsd_service_info.GetPort()};

  // Add TextRecords from NsdServiceInfo
  auto text_attributes = dnssd_service_instance_.TextAttributes();

  absl::flat_hash_map<std::string, std::string> text_records =
      nsd_service_info.GetTxtRecords();
  auto it = text_records.begin();
  while (it != text_records.end()) {
    text_attributes.Insert(string_utils::StringToWideString(it->first),
                           string_utils::StringToWideString(it->second));
    it++;
  }

  // Add IPv4 address in text attributes.
  std::vector<std::string> ipv4_addresses = GetIpv4Addresses();
  if (!ipv4_addresses.empty()) {
    if (ipv4_addresses.size() > 1) {
      LOG(WARNING) << "The device has multiple IPv4 addresses.";
    }
    text_attributes.Insert(winrt::to_hstring(std::string(kDeviceIpv4)),
                           winrt::to_hstring(ipv4_addresses[0]));
  }

  dnssd_regirstraion_result_ = dnssd_service_instance_
                                   .RegisterStreamSocketListenerAsync(
                                       server_socket_ptr->GetSocketListener())
                                   .get();

  if (dnssd_regirstraion_result_.HasInstanceNameChanged()) {
    LOG(WARNING) << "advertising instance name was changed due to have "
                    "same name instance was running.";
    // stop the service and return false
    StopAdvertising(nsd_service_info);
    return false;
  }

  if (dnssd_regirstraion_result_.Status() == DnssdRegistrationStatus::Success) {
    LOG(INFO) << "started to advertising.";
    medium_status_ |= kMediumStatusAdvertising;
    return true;
  }

  // Clean up
  LOG(ERROR) << "failed to start advertising due to registration failure.";
  dnssd_service_instance_ = nullptr;
  dnssd_regirstraion_result_ = nullptr;
  return false;
}

// Win32 call only can use globel function or static method in class
void WifiLanMedium::Advertising_StopCompleted(DWORD Status, PVOID pQueryContext,
                                              PDNS_SERVICE_INSTANCE pInstance) {
  LOG(INFO) << "unregister with status=" << Status;
  try {
    WifiLanMedium* medium = static_cast<WifiLanMedium*>(pQueryContext);
    medium->NotifyDnsServiceUnregistered(Status);
  } catch (...) {
    LOG(ERROR) << "failed to notify the stop of DNS service instance."
               << Status;
  }
}

void WifiLanMedium::NotifyDnsServiceUnregistered(DWORD status) {
  if (dns_service_stop_latch_.get() != nullptr) {
    dns_service_stop_status_ = status;
    dns_service_stop_latch_.get()->CountDown();
  }
}

bool WifiLanMedium::StopAdvertising(const NsdServiceInfo& nsd_service_info) {
  // Need to use Win32 API to deregister the Dnssd instance
  if (!IsAdvertising()) {
    LOG(WARNING)
        << "Cannot stop advertising because no advertising is running.";
    return false;
  }

  // Init DNS service instance
  std::string instance_name = absl::StrFormat(
      kMdnsInstanceNameFormat.data(), nsd_service_info.GetServiceName(),
      nsd_service_info.GetServiceType());
  int port = nsd_service_info.GetPort();
  dns_service_instance_name_ = std::make_unique<std::wstring>(
      string_utils::StringToWideString(instance_name));

  dns_service_instance_.pszInstanceName =
      (LPWSTR)dns_service_instance_name_->c_str();
  dns_service_instance_.pszHostName = (LPWSTR)kMdnsHostName.data();
  dns_service_instance_.wPort = port;

  // Init DNS service register request
  dns_service_register_request_.Version = DNS_QUERY_REQUEST_VERSION1;
  dns_service_register_request_.InterfaceIndex =
      0;  // all interfaces will be considered
  dns_service_register_request_.unicastEnabled = false;
  dns_service_register_request_.hCredentials = nullptr;
  dns_service_register_request_.pServiceInstance = &dns_service_instance_;
  dns_service_register_request_.pQueryContext = this;  // callback use it
  dns_service_register_request_.pRegisterCompletionCallback =
      WifiLanMedium::Advertising_StopCompleted;

  dns_service_stop_latch_ = std::make_unique<CountDownLatch>(1);
  DWORD status = DnsServiceDeRegister(&dns_service_register_request_, nullptr);

  if (status != DNS_REQUEST_PENDING) {
    LOG(ERROR) << "failed to stop mDNS advertising for service type ="
               << nsd_service_info.GetServiceType();
    return false;
  }

  // Wait for stop finish
  dns_service_stop_latch_.get()->Await();
  dns_service_stop_latch_ = nullptr;
  if (dns_service_stop_status_ != 0) {
    LOG(INFO) << "failed to stop mDNS advertising for service type ="
              << nsd_service_info.GetServiceType();
    return false;
  }

  LOG(INFO) << "succeeded to stop mDNS advertising for service type ="
            << nsd_service_info.GetServiceType();
  medium_status_ &= (~kMediumStatusAdvertising);
  return true;
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
  LOG(ERROR) << "connect to service by NSD service info. service type is "
             << remote_service_info.GetServiceType();

  return ConnectToService(remote_service_info.GetIPAddress(),
                          remote_service_info.GetPort(), cancellation_flag);
}

std::unique_ptr<api::WifiLanSocket> WifiLanMedium::ConnectToService(
    const std::string& ip_address, int port,
    CancellationFlag* cancellation_flag) {
  LOG(INFO) << "ConnectToService is called.";
  if (ip_address.empty() || ip_address.length() != 4 || port == 0) {
    LOG(ERROR) << "no valid service address and port to connect.";
    return nullptr;
  }

  // Converts ip address to x.x.x.x format
  in_addr address;
  address.S_un.S_un_b.s_b1 = ip_address[0];
  address.S_un.S_un_b.s_b2 = ip_address[1];
  address.S_un.S_un_b.s_b3 = ip_address[2];
  address.S_un.S_un_b.s_b4 = ip_address[3];
  char* ipv4_address = inet_ntoa(address);
  if (ipv4_address == nullptr) {
    LOG(ERROR) << "Invalid IP address parameter.";
    return nullptr;
  }

  std::unique_ptr<CancellationFlagListener> connection_cancellation_listener =
      nullptr;

  HostName host_name{
      string_utils::StringToWideString(std::string(ipv4_address))};
  winrt::hstring service_name{winrt::to_hstring(port)};

  StreamSocket socket{};

  // setup cancel listener
  if (cancellation_flag != nullptr) {
    if (cancellation_flag->Cancelled()) {
      LOG(INFO) << "connect has been cancelled to service " << ipv4_address
                << ":" << port;
      return nullptr;
    }

    connection_cancellation_listener =
        std::make_unique<nearby::CancellationFlagListener>(
            cancellation_flag, [socket]() {
              LOG(WARNING) << "connect is closed due to it is cancelled.";
              socket.Close();
            });
  }

  // connection to the service
  try {
    if (FeatureFlags::GetInstance().GetFlags().enable_connection_timeout) {
      connection_timeout_ = scheduled_executor_.Schedule(
          [socket]() {
            LOG(WARNING) << "connect is closed due to timeout.";
            socket.Close();
          },
          kConnectServiceTimeout);
    }

    socket.ConnectAsync(host_name, service_name).get();

    if (connection_timeout_ != nullptr) {
      connection_timeout_->Cancel();
      connection_timeout_ = nullptr;
    }

    std::unique_ptr<WifiLanSocket> wifi_lan_socket =
        std::make_unique<WifiLanSocket>(socket);

    std::string local_address =
        winrt::to_string(socket.Information().LocalAddress().DisplayName());
    std::string local_port = winrt::to_string(socket.Information().LocalPort());

    LOG(INFO) << "connected to remote service " << ipv4_address << ":" << port
              << " with local address " << local_address << ":" << local_port;
    return wifi_lan_socket;
  } catch (...) {
    LOG(ERROR) << "failed to connect remote service " << ipv4_address << ":"
               << port;
  }

  if (connection_timeout_ != nullptr) {
    connection_timeout_->Cancel();
    connection_timeout_ = nullptr;
  }

  return nullptr;
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

  if (server_socket->listen()) {
    int port = server_socket_ptr->GetPort();
    LOG(INFO) << "started to listen serive on IP:port "
              << ipaddr_4bytes_to_dotdecimal_string(
                     server_socket_ptr->GetIPAddress())
              << ":" << port;
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
    LOG(WARNING) << "no service name information in device information.";
    return Exception{Exception::kFailed};
  }
  nsd_service_info.SetServiceName(InspectableReader::ReadString(inspectable));

  // Read service type information
  inspectable = properties.TryLookup(L"System.Devices.Dnssd.ServiceName");
  if (inspectable == nullptr) {
    LOG(WARNING) << "no service type information in device information.";
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
    LOG(WARNING) << "No text attributes information in device information.";
    return Exception{Exception::kFailed};
  }

  auto text_attributes = InspectableReader::ReadStringArray(inspectable);
  for (auto text_attribute : text_attributes) {
    // text attribute in format key=value
    int pos = text_attribute.find("=");
    if (pos <= 0 || pos == text_attribute.size() - 1) {
      LOG(WARNING) << "found invalid text attribute " << text_attribute;
      continue;
    }

    std::string key = text_attribute.substr(0, pos);
    std::string value = text_attribute.substr(pos + 1);
    nsd_service_info.SetTxtRecord(key, value);
  }

  if (!is_device_found) {
    return ExceptionOr<NsdServiceInfo>(nsd_service_info);
  }

  // Read IP Address information
  std::string ip_address;
  ip_address.resize(4);

  // Find it from text first
  std::vector<std::string> ip_address_candidates;

  std::string ipv4_address =
      nsd_service_info.GetTxtRecord(std::string(kDeviceIpv4));
  if (!ipv4_address.empty()) {
    ip_address_candidates.push_back(ipv4_address);
  } else {
    inspectable = properties.TryLookup(L"System.Devices.IPAddress");
    if (inspectable == nullptr) {
      LOG(WARNING) << "No IP address property in device information.";
      return Exception{Exception::kFailed};
    }
    ip_address_candidates = InspectableReader::ReadStringArray(inspectable);
  }

  if (ip_address_candidates.empty()) {
    LOG(WARNING) << "No IP address information in device information.";
    return Exception{Exception::kFailed};
  }

  // Gets 4 bytes string
  for (std::string& address : ip_address_candidates) {
    uint32_t addr = inet_addr(address.data());
    if (addr == INADDR_NONE) {
      continue;
    }

    in_addr ipv4_addr;
    ipv4_addr.S_un.S_addr = addr;
    ip_address[0] = static_cast<char>(ipv4_addr.S_un.S_un_b.s_b1);
    ip_address[1] = static_cast<char>(ipv4_addr.S_un.S_un_b.s_b2);
    ip_address[2] = static_cast<char>(ipv4_addr.S_un.S_un_b.s_b3);
    ip_address[3] = static_cast<char>(ipv4_addr.S_un.S_un_b.s_b4);
    break;
  }

  nsd_service_info.SetIPAddress(ip_address);

  // Read IP port
  inspectable = properties.TryLookup(L"System.Devices.Dnssd.PortNumber");
  if (inspectable == nullptr) {
    LOG(WARNING) << "no IP port property in device information.";
    return Exception{Exception::kFailed};
  }

  int port = InspectableReader::ReadUint16(inspectable);
  nsd_service_info.SetPort(port);

  return ExceptionOr<NsdServiceInfo>(nsd_service_info);
}

fire_and_forget WifiLanMedium::Watcher_DeviceAdded(
    DeviceWatcher sender, DeviceInformation deviceInfo) {
  // need to read IP address and port information from deviceInfo
  ExceptionOr<NsdServiceInfo> nsd_service_info_except =
      GetNsdServiceInformation(deviceInfo.Properties(),
                               /*is_device_found*/ true);

  if (!nsd_service_info_except.ok()) {
    LOG(WARNING) << "NSD information is incompleted or has error! "
                    "Don't add WIFI_LAN device.";
    return fire_and_forget{};
  }

  NsdServiceInfo nsd_service_info = nsd_service_info_except.GetResult();
  std::string endpoint =
      nsd_service_info.GetTxtRecord(kDeviceEndpointInfo.data());
  if (endpoint.empty()) {
    LOG(WARNING) << "No endpoint information! "
                    "Don't add WIFI_LAN device.";
    return fire_and_forget{};
  }

  // Don't discover itself
  if (nsd_service_info.GetServiceName() == service_name_) {
    LOG(WARNING) << "Don't add WIFI_LAN device for itself";
    return fire_and_forget{};
  }

  LOG(INFO) << "device added for service name "
            << nsd_service_info.GetServiceName() << ", address: "
            << ipaddr_4bytes_to_dotdecimal_string(
                   nsd_service_info.GetIPAddress())
            << ":" << nsd_service_info.GetPort();

  if (!IsConnectableIpAddress(
          ipaddr_4bytes_to_dotdecimal_string(nsd_service_info.GetIPAddress()),
          nsd_service_info.GetPort(), kConnectTimeout)) {
    LOG(WARNING) << "Don't add WIFI_LAN device due to it is not reachable.";
    return fire_and_forget{};
  }

  UpdateDiscoveredService(winrt::to_string(deviceInfo.Id()), nsd_service_info);
  discovered_service_callback_.service_discovered_cb(nsd_service_info);

  return fire_and_forget();
}

fire_and_forget WifiLanMedium::Watcher_DeviceUpdated(
    DeviceWatcher sender, DeviceInformationUpdate deviceInfoUpdate) {
  ExceptionOr<NsdServiceInfo> nsd_service_info_except =
      GetNsdServiceInformation(deviceInfoUpdate.Properties(),
                               /*is_device_found*/ true);

  if (!nsd_service_info_except.ok()) {
    LOG(WARNING) << "NSD information is incompleted or has error!";
    return fire_and_forget{};
  }

  NsdServiceInfo nsd_service_info = nsd_service_info_except.GetResult();

  // Don't discover itself
  if (nsd_service_info.GetServiceName() == service_name_) {
    LOG(WARNING) << "Don't update WIFI_LAN device for itself.";
    return fire_and_forget{};
  }

  // check having any changes
  std::optional<NsdServiceInfo> last_nsd_service_info =
      GetDiscoveredService(winrt::to_string(deviceInfoUpdate.Id()));
  if (!last_nsd_service_info.has_value()) {
    LOG(WARNING)
        << "Don't update WIFI_LAN device due to it is not in device list.";
    return fire_and_forget{};
  }

  if ((last_nsd_service_info->GetTxtRecord(std::string(kDeviceEndpointInfo)) ==
       nsd_service_info.GetTxtRecord(std::string(kDeviceEndpointInfo))) &&
      (last_nsd_service_info->GetServiceName() ==
       nsd_service_info.GetServiceName()) &&
      (last_nsd_service_info->GetIPAddress() ==
       nsd_service_info.GetIPAddress()) &&
      (last_nsd_service_info->GetPort() == nsd_service_info.GetPort())) {
    LOG(INFO) << "Don't update WIFI_LAN device due to no change.";
    return fire_and_forget{};
  }

  LOG(INFO)
      << "Device is changed from (service name:"
      << last_nsd_service_info->GetServiceName() << ", endpoint info:"
      << last_nsd_service_info->GetTxtRecord(std::string(kDeviceEndpointInfo))
      << ", address:"
      << ipaddr_4bytes_to_dotdecimal_string(
             last_nsd_service_info->GetIPAddress())
      << ":" << last_nsd_service_info->GetPort()
      << ") to (service name:" << nsd_service_info.GetServiceName() << ", "
      << nsd_service_info.GetTxtRecord(std::string(kDeviceEndpointInfo))
      << ", address:"
      << ipaddr_4bytes_to_dotdecimal_string(nsd_service_info.GetIPAddress())
      << ":" << nsd_service_info.GetPort() << ").";

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
  // need to read IP address and port information from deviceInfo
  ExceptionOr<NsdServiceInfo> nsd_service_info_except =
      GetNsdServiceInformation(deviceInfoUpdate.Properties(),
                               /*is_device_found*/ false);

  if (!nsd_service_info_except.ok()) {
    LOG(WARNING) << "NSD information is incompleted or has error! Ignore";
    return fire_and_forget{};
  }

  NsdServiceInfo nsd_service_info = nsd_service_info_except.GetResult();
  LOG(INFO) << "device removed for service name "
            << nsd_service_info.GetServiceName();

  std::string endpoint =
      nsd_service_info.GetTxtRecord(kDeviceEndpointInfo.data());
  if (endpoint.empty()) {
    return fire_and_forget{};
  }

  RemoveDiscoveredService(winrt::to_string(deviceInfoUpdate.Id()));
  discovered_service_callback_.service_lost_cb(nsd_service_info);

  return fire_and_forget();
}

void WifiLanMedium::ClearDiscoveredServices() {
  absl::MutexLock lock(&mutex_);
  discovered_services_map_.clear();
}

std::optional<NsdServiceInfo> WifiLanMedium::GetDiscoveredService(
    absl::string_view id) {
  absl::MutexLock lock(&mutex_);
  auto it = discovered_services_map_.find(id);
  if (it == discovered_services_map_.end()) {
    return std::nullopt;
  }

  return it->second;
}

void WifiLanMedium::UpdateDiscoveredService(
    absl::string_view id, const NsdServiceInfo& nsd_service_info) {
  absl::MutexLock lock(&mutex_);
  discovered_services_map_[id] = nsd_service_info;
}

void WifiLanMedium::RemoveDiscoveredService(absl::string_view id) {
  absl::MutexLock lock(&mutex_);
  auto it = discovered_services_map_.find(id);
  if (it != discovered_services_map_.end()) {
    discovered_services_map_.erase(it);
  }
}

bool WifiLanMedium::IsConnectableIpAddress(absl::string_view ip, int port,
                                           absl::Duration timeout) {
  bool result = false;
  int error = -1;
  int size = sizeof(int);
  timeval tm;
  fd_set set;
  unsigned long non_blocking = 1;  // NOLINT
  struct sockaddr_in serv_addr;
  SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port);
  serv_addr.sin_addr.S_un.S_addr = inet_addr(std::string(ip).c_str());

  ioctlsocket(sock, /*cmd=*/FIONBIO, /*argp=*/&non_blocking);
  if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) ==
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

std::string WifiLanMedium::GetErrorMessage(std::exception_ptr eptr) {
  try {
    if (eptr) {
      std::rethrow_exception(eptr);
    } else {
      return "";
    }
  } catch (const std::exception& e) {
    return e.what();
  }
}

}  // namespace windows
}  // namespace nearby
