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

// Standard C/C++ headers
#include <codecvt>
#include <cstdint>
#include <locale>
#include <string>

// ABSL headers
#include "absl/strings/str_format.h"

// Nearby connections headers
#include "internal/platform/cancellation_flag_listener.h"
#include "internal/platform/implementation/windows/utils.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex_lock.h"

namespace location {
namespace nearby {
namespace windows {

bool WifiLanMedium::StartAdvertising(const NsdServiceInfo& nsd_service_info) {
  absl::MutexLock lock(&mutex_);

  if (!IsAccepting()) {
    NEARBY_LOGS(WARNING)
        << "cannot start advertising without accepting connetions.";
    return false;
  }

  if (IsAdvertising()) {
    NEARBY_LOGS(WARNING)
        << "cannot start advertising again when it is running.";
    return false;
  }

  if (nsd_service_info.GetTxtRecord(KEY_ENDPOINT_INFO.data()).empty()) {
    NEARBY_LOGS(ERROR) << "cannot start advertising without endpoint info.";
    return false;
  }

  if (nsd_service_info.GetServiceName().empty()) {
    NEARBY_LOGS(ERROR) << "cannot start advertising without service name.";
    return false;
  }

  std::string instance_name = absl::StrFormat(
      MDNS_INSTANCE_NAME_FORMAT.data(), nsd_service_info.GetServiceName(),
      nsd_service_info.GetServiceType());

  NEARBY_LOGS(INFO) << "mDNS instance name is " << instance_name;

  dnssd_service_instance_ = DnssdServiceInstance{
      string_to_wstring(instance_name),
      nullptr,  // let windows use default computer's local name
      (uint16)nsd_service_info.GetPort()};

  // Add TextRecords from NsdServiceInfo
  auto text_attributes = dnssd_service_instance_.TextAttributes();

  auto text_records = nsd_service_info.GetTxtRecords();
  auto it = text_records.begin();
  while (it != text_records.end()) {
    text_attributes.Insert(string_to_wstring(it->first),
                           string_to_wstring(it->second));
    it++;
  }

  dnssd_regirstraion_result_ = dnssd_service_instance_
                                   .RegisterStreamSocketListenerAsync(
                                       server_socket_ptr_->GetSocketListener())
                                   .get();

  if (dnssd_regirstraion_result_.HasInstanceNameChanged()) {
    NEARBY_LOGS(WARNING) << "advertising instance name was changed due to have "
                            "same name instance was running.";
    // stop the service and return false
    StopAdvertising(nsd_service_info);
    return false;
  }

  if (dnssd_regirstraion_result_.Status() == DnssdRegistrationStatus::Success) {
    NEARBY_LOGS(INFO) << "started to advertising.";
    medium_status_ |= MEDIUM_STATUS_ADVERTISING;
    return true;
  }

  // Clean up
  NEARBY_LOGS(ERROR)
      << "failed to start advertising due to registration failure.";
  dnssd_service_instance_ = nullptr;
  dnssd_regirstraion_result_ = nullptr;
  return false;
}

// Win32 call only can use globel function or static method in class
void WifiLanMedium::Advertising_StopCompleted(DWORD Status, PVOID pQueryContext,
                                              PDNS_SERVICE_INSTANCE pInstance) {
  NEARBY_LOGS(INFO) << "unregister with status=" << Status;
  try {
    WifiLanMedium* medium = static_cast<WifiLanMedium*>(pQueryContext);
    medium->NotifyDnsServiceUnregistered(Status);
  } catch (...) {
    NEARBY_LOGS(ERROR) << "failed to notify the stop of DNS service instance."
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
    NEARBY_LOGS(WARNING)
        << "Cannot stop advertising because no advertising is running.";
    return false;
  }

  // Init DNS service instance
  std::string instance_name = absl::StrFormat(
      MDNS_INSTANCE_NAME_FORMAT.data(), nsd_service_info.GetServiceName(),
      nsd_service_info.GetServiceType());
  int port = nsd_service_info.GetPort();
  dns_service_instance_name_ =
      std::make_unique<std::wstring>(string_to_wstring(instance_name));

  dns_service_instance_.pszInstanceName =
      (LPWSTR)dns_service_instance_name_->c_str();
  dns_service_instance_.pszHostName = (LPWSTR)MDNS_HOST_NAME.data();
  dns_service_instance_.wPort = port;

  // Init DNS service register request
  dns_service_register_request_.Version = DNS_QUERY_REQUEST_VERSION1;
  dns_service_register_request_.InterfaceIndex =
      0;  // all interfaces will be considered
  dns_service_register_request_.unicastEnabled = false;
  dns_service_register_request_.hCredentials = NULL;
  dns_service_register_request_.pServiceInstance = &dns_service_instance_;
  dns_service_register_request_.pQueryContext = this;  // callback use it
  dns_service_register_request_.pRegisterCompletionCallback =
      WifiLanMedium::Advertising_StopCompleted;

  dns_service_stop_latch_ = std::make_unique<CountDownLatch>(1);
  DWORD status = DnsServiceDeRegister(&dns_service_register_request_, nullptr);

  if (status != DNS_REQUEST_PENDING) {
    NEARBY_LOGS(ERROR) << "failed to stop mDNS advertising for service type ="
                       << nsd_service_info.GetServiceType();
    return false;
  }

  // Wait for stop finish
  dns_service_stop_latch_.get()->Await();
  dns_service_stop_latch_ = nullptr;
  if (dns_service_stop_status_ != 0) {
    NEARBY_LOGS(INFO) << "failed to stop mDNS advertising for service type ="
                      << nsd_service_info.GetServiceType();
    return false;
  }

  NEARBY_LOGS(INFO) << "succeeded to stop mDNS advertising for service type ="
                    << nsd_service_info.GetServiceType();
  medium_status_ &= (~MEDIUM_STATUS_ADVERTISING);
  return true;
}

// Returns true once the WifiLan discovery has been initiated.
bool WifiLanMedium::StartDiscovery(const std::string& service_type,
                                   DiscoveredServiceCallback callback) {
  if (IsDiscovering()) {
    NEARBY_LOGS(WARNING) << "discovery already running for service type ="
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
      absl::StrFormat(MDNS_DEVICE_SELECTOR_FORMAT.data(), service_type_trim);

  std::vector<winrt::hstring> requestedProperties{
      L"System.Devices.IpAddress",
      L"System.Devices.Dnssd.HostName",
      L"System.Devices.Dnssd.InstanceName",
      L"System.Devices.Dnssd.PortNumber",
      L"System.Devices.Dnssd.ServiceName",
      L"System.Devices.Dnssd.TextAttributes"};

  device_watcher_ = DeviceInformation::CreateWatcher(
      string_to_wstring(selector), requestedProperties,
      DeviceInformationKind::AssociationEndpointService);

  device_watcher_added_event_token =
      device_watcher_.Added({this, &WifiLanMedium::Watcher_DeviceAdded});
  device_watcher_updated_event_token =
      device_watcher_.Updated({this, &WifiLanMedium::Watcher_DeviceUpdated});
  device_watcher_removed_event_token =
      device_watcher_.Removed({this, &WifiLanMedium::Watcher_DeviceRemoved});

  device_watcher_.Start();
  discovered_service_callback_ = std::move(callback);
  medium_status_ |= MEDIUM_STATUS_DISCOVERING;

  NEARBY_LOGS(INFO) << "started to discovery.";

  return true;
}

// Returns true once WifiLan discovery for service_id is well and truly
// stopped; after this returns, there must be no more invocations of the
// DiscoveredServiceCallback passed in to StartDiscovery() for service_id.
bool WifiLanMedium::StopDiscovery(const std::string& service_type) {
  if (!IsDiscovering()) {
    NEARBY_LOGS(WARNING) << "no discovering service to stop.";
    return false;
  }
  device_watcher_.Stop();
  device_watcher_.Added(device_watcher_added_event_token);
  device_watcher_.Updated(device_watcher_updated_event_token);
  device_watcher_.Removed(device_watcher_removed_event_token);
  medium_status_ &= (~MEDIUM_STATUS_DISCOVERING);
  device_watcher_ = nullptr;
  return true;
}

std::unique_ptr<api::WifiLanSocket> WifiLanMedium::ConnectToService(
    const NsdServiceInfo& remote_service_info,
    CancellationFlag* cancellation_flag) {
  NEARBY_LOGS(ERROR)
      << "connect to service by NSD service info. service type is "
      << remote_service_info.GetServiceType();

  return ConnectToService(remote_service_info.GetIPAddress(),
                          remote_service_info.GetPort(), cancellation_flag);
}

std::unique_ptr<api::WifiLanSocket> WifiLanMedium::ConnectToService(
    const std::string& ip_address, int port,
    CancellationFlag* cancellation_flag) {
  if (ip_address.empty() || ip_address.length() != 4 || port == 0) {
    NEARBY_LOGS(ERROR) << "no valid service address and port to connect.";
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
    NEARBY_LOGS(ERROR) << "Invalid IP address parameter.";
    return nullptr;
  }

  HostName host_name{string_to_wstring(std::string(ipv4_address))};
  winrt::hstring service_name{winrt::to_hstring(port)};

  StreamSocket socket{};

  // setup cancel listener
  if (cancellation_flag != nullptr) {
    if (cancellation_flag->Cancelled()) {
      NEARBY_LOGS(INFO) << "connect has been cancelled to service "
                        << ipv4_address << ":" << port;
      return nullptr;
    }

    location::nearby::CancellationFlagListener cancellationFlagListener(
        cancellation_flag, [socket]() { socket.CancelIOAsync().get(); });
  }

  // connection to the service
  try {
    socket.ConnectAsync(host_name, service_name).get();
    // connected need to keep connection

    std::unique_ptr<WifiLanSocket> wifi_lan_socket =
        std::make_unique<WifiLanSocket>(socket);

    NEARBY_LOGS(INFO) << "connected to remote service " << ipv4_address << ":"
                      << port;
    return wifi_lan_socket;
  } catch (...) {
    NEARBY_LOGS(ERROR) << "failed to connect remote service " << ipv4_address
                       << ":" << port;
  }

  return nullptr;
}

std::unique_ptr<api::WifiLanServerSocket> WifiLanMedium::ListenForService(
    int port) {
  absl::MutexLock lock(&mutex_);

  // check current status
  if (IsAccepting()) {
    NEARBY_LOGS(WARNING) << "accepting connections already started on port "
                         << server_socket_ptr_->GetPort();
    return nullptr;
  }

  std::unique_ptr<WifiLanServerSocket> server_socket =
      std::make_unique<WifiLanServerSocket>(port);
  server_socket_ptr_ = server_socket.get();

  server_socket->SetCloseNotifier([this]() {
    absl::MutexLock lock(&mutex_);
    NEARBY_LOGS(INFO) << "server socket was closed on port "
                      << server_socket_ptr_->GetPort();
    medium_status_ &= (~MEDIUM_STATUS_ACCEPTING);
    server_socket_ptr_ = nullptr;
  });

  if (server_socket->listen()) {
    medium_status_ |= MEDIUM_STATUS_ACCEPTING;
    NEARBY_LOGS(INFO) << "started to listen serive on port " << port;
    return server_socket;
  }

  NEARBY_LOGS(ERROR) << "Failed to listen service on port " << port;

  return nullptr;
}

NsdServiceInfo WifiLanMedium::GetNsdServiceInformation(
    IMapView<winrt::hstring, IInspectable> properties) {
  NsdServiceInfo nsd_service_info{};

  // Service name information
  IInspectable inspectable =
      properties.TryLookup(L"System.Devices.Dnssd.InstanceName");
  if (inspectable == nullptr) {
    NEARBY_LOGS(WARNING)
        << "no service name information in device information.";
    return nsd_service_info;
  }
  nsd_service_info.SetServiceName(InspectableReader::ReadString(inspectable));

  // Service type information
  inspectable = properties.TryLookup(L"System.Devices.Dnssd.ServiceName");
  if (inspectable == nullptr) {
    NEARBY_LOGS(WARNING)
        << "no service type information in device information.";
    return nsd_service_info;
  }

  // In WifiLan::StartDiscovery(), service_type is appended with "._tcp." for
  // ios and android platform. For windows, we only have "._tcp" as appendix.
  // Here "." is added back to match the upper layer service_type, because
  // service_type is used to get the corresponding call back function.
  nsd_service_info.SetServiceType(
      (InspectableReader::ReadString(inspectable)).append("."));

  // IP Address information
  inspectable = properties.TryLookup(L"System.Devices.IPAddress");
  if (inspectable == nullptr) {
    NEARBY_LOGS(WARNING) << "no IP address information in device information.";
    return nsd_service_info;
  }

  auto ipaddresses = InspectableReader::ReadStringArray(inspectable);
  if (ipaddresses.size() == 0) {
    NEARBY_LOGS(WARNING) << "no IP address information in device information.";
    return nsd_service_info;
  }

  std::string ip_address;
  ip_address.resize(4);
  // Gets 4 bytes string
  for (std::string& address : ipaddresses) {
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

  // read IP port
  inspectable = properties.TryLookup(L"System.Devices.Dnssd.PortNumber");
  if (inspectable == nullptr) {
    NEARBY_LOGS(WARNING) << "no IP port information in device information.";
    return nsd_service_info;
  }

  int port = InspectableReader::ReadUint16(inspectable);
  nsd_service_info.SetIPAddress(ip_address);
  nsd_service_info.SetPort(port);

  // read text record
  inspectable = properties.TryLookup(L"System.Devices.Dnssd.TextAttributes");
  if (inspectable == nullptr) {
    NEARBY_LOGS(WARNING)
        << "no text attributes information in device information.";
    return nsd_service_info;
  }

  auto text_attributes = InspectableReader::ReadStringArray(inspectable);
  for (auto text_attribute : text_attributes) {
    // text attribute in format key=value
    int pos = text_attribute.find("=");
    if (pos <= 0 || pos == text_attribute.size() - 1) {
      NEARBY_LOGS(WARNING) << "found invalid text attribute " << text_attribute;
      continue;
    }

    std::string key = text_attribute.substr(0, pos);
    std::string value = text_attribute.substr(pos + 1);
    nsd_service_info.SetTxtRecord(key, value);
  }

  return nsd_service_info;
}

fire_and_forget WifiLanMedium::Watcher_DeviceAdded(
    DeviceWatcher sender, DeviceInformation deviceInfo) {
  // need to read IP address and port information from deviceInfo
  NsdServiceInfo nsd_service_info =
      GetNsdServiceInformation(deviceInfo.Properties());

  NEARBY_LOGS(INFO) << "device added for service name "
                    << nsd_service_info.GetServiceName();

  std::string endpoint =
      nsd_service_info.GetTxtRecord(KEY_ENDPOINT_INFO.data());
  if (endpoint.empty()) {
    return fire_and_forget{};
  }

  discovered_service_callback_.service_discovered_cb(nsd_service_info);

  return fire_and_forget();
}
fire_and_forget WifiLanMedium::Watcher_DeviceUpdated(
    DeviceWatcher sender, DeviceInformationUpdate deviceInfoUpdate) {
  // TODO(b/200421481): discovery servcie callback needs to support device
  // update.
  NsdServiceInfo nsd_service_info =
      GetNsdServiceInformation(deviceInfoUpdate.Properties());
  NEARBY_LOGS(INFO) << "device updated for service name "
                    << nsd_service_info.GetServiceName();

  return fire_and_forget();
}
fire_and_forget WifiLanMedium::Watcher_DeviceRemoved(
    DeviceWatcher sender, DeviceInformationUpdate deviceInfoUpdate) {
  // need to read IP address and port information from deviceInfo
  NsdServiceInfo nsd_service_info =
      GetNsdServiceInformation(deviceInfoUpdate.Properties());

  NEARBY_LOGS(INFO) << "device removed for service name "
                    << nsd_service_info.GetServiceName();

  std::string endpoint =
      nsd_service_info.GetTxtRecord(KEY_ENDPOINT_INFO.data());
  if (endpoint.empty()) {
    return fire_and_forget{};
  }

  discovered_service_callback_.service_lost_cb(nsd_service_info);

  return fire_and_forget();
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
}  // namespace location
