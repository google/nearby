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

// Absl headers
#include "absl/strings/str_format.h"

// Nearby connections headers
#include "platform/impl/windows/utils.h"
#include "platform/impl/windows/wifi_lan.h"
#include "platform/public/logging.h"
#include "platform/public/mutex_lock.h"

// WinRT headers
#include "platform/impl/windows/generated/winrt/Windows.Foundation.Collections.h"

namespace location {
namespace nearby {
namespace windows {

WifiLanNsd::WifiLanNsd(WifiLanMedium* medium, const std::string service_id) {
  medium_ = medium;
  service_id_ = service_id;
  service_type_ = GetServiceIdHash();
}

bool WifiLanNsd::StartAcceptingConnections(
    api::WifiLanMedium::AcceptedConnectionCallback callback) {
  // TODO: check windows version to decide whether support mDNS service

  // Check IP address
  ip_addresses_ = GetIpAddresses();

  if (ip_addresses_.empty()) {
    NEARBY_LOGS(WARNING) << "failed to start accepting connection without IP "
                            "addreses configured on computer.";
    return false;
  }

  // check current status
  if (IsAccepting()) {
    NEARBY_LOGS(WARNING) << "accepting connections already started. service id="
                         << service_id_;
    return false;
  }

  int port = GenerateSocketPort(service_id_);

  // Save connection callback
  accepted_connection_callback_ = std::move(callback);
  stream_socket_listener_ = StreamSocketListener();

  // Setup callback
  listener_event_token_ = stream_socket_listener_.ConnectionReceived(
      {this, &WifiLanNsd::Listener_ConnectionReceived});

  try {
    stream_socket_listener_.BindServiceNameAsync(winrt::to_hstring(port)).get();
    nsd_status_ |= NSD_STATUS_ACCEPTING;
    return true;
  } catch (...) {
    // Cannot bind to the preferred port, will let system to assign port.
    NEARBY_LOGS(WARNING) << "cannot accept connection on preferred port.";
  }

  try {
    stream_socket_listener_.BindServiceNameAsync({}).get();
    // need to save the port information
    port = std::stoi(stream_socket_listener_.Information().LocalPort().c_str());
    nsd_status_ |= NSD_STATUS_ACCEPTING;
    return true;
  } catch (...) {
    // Cannot bind to the preferred port, will let system to assign port.
    NEARBY_LOGS(ERROR) << "cannot bind to any port.";
  }

  // clean up
  stream_socket_listener_.ConnectionReceived(listener_event_token_);
  stream_socket_listener_ = nullptr;

  return false;
}

bool WifiLanNsd::StopAcceptingConnections() {
  if (!IsAccepting()) {
    NEARBY_LOGS(WARNING) << "no accepting connections to stop.";
    return false;
  }

  stream_socket_listener_.ConnectionReceived(listener_event_token_);
  stream_socket_listener_ = nullptr;
  nsd_status_ &= (~NSD_STATUS_ACCEPTING);
  return true;
}

bool WifiLanNsd::StartAdvertising(const NsdServiceInfo& nsd_service_info) {
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

  if (nsd_service_info.GetServiceInfoName().empty()) {
    NEARBY_LOGS(ERROR) << "cannot start advertising without service name.";
    return false;
  }

  // Setup WiFi LAN service information
  int port =
      std::stoi(stream_socket_listener_.Information().LocalPort().c_str());
  NsdServiceInfo new_nsd_servcie_info = nsd_service_info;
  // TODO: need feature enhancement to support multiple network interfaces
  new_nsd_servcie_info.SetServiceAddress(ip_addresses_[0], port);
  wifi_lan_service_ = WifiLanService(new_nsd_servcie_info);
  wifi_lan_service_.SetMedium(medium_);

  std::string instance_name = absl::StrFormat(
      MDNS_INSTANCE_NAME_FORMAT.data(),
      wifi_lan_service_.GetServiceInfo().GetServiceInfoName(), service_type_);

  NEARBY_LOGS(INFO) << "mDNS instance name is " << instance_name;

  dnssd_service_instance_ = DnssdServiceInstance{
      string_to_wstring(instance_name),
      nullptr,  // let windows use default computer's local name
      (uint16)port};

  // Add TextRecords from NsdServiceInfo
  auto text_attributes = dnssd_service_instance_.TextAttributes();
  // TODO(b/200298824): NsdServiceInfo should have function to return all text
  // records will be more generic
  text_attributes.Insert(string_to_wstring(KEY_ENDPOINT_INFO.data()),
                         string_to_wstring(nsd_service_info.GetTxtRecord(
                             KEY_ENDPOINT_INFO.data())));

  dnssd_regirstraion_result_ =
      dnssd_service_instance_
          .RegisterStreamSocketListenerAsync(stream_socket_listener_)
          .get();

  if (dnssd_regirstraion_result_.HasInstanceNameChanged()) {
    NEARBY_LOGS(WARNING) << "advertising instance name was changed due to have "
                            "same name instance was running.";
    // stop the service and return false
    StopAdvertising();
    return false;
  }

  if (dnssd_regirstraion_result_.Status() == DnssdRegistrationStatus::Success) {
    NEARBY_LOGS(INFO) << "started to advertising.";
    nsd_status_ |= NSD_STATUS_ADVERTISING;
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
void WifiLanNsd::Advertising_StopCompleted(DWORD Status, PVOID pQueryContext,
                                           PDNS_SERVICE_INSTANCE pInstance) {
  NEARBY_LOGS(INFO) << "unregister with status=" << Status;
  try {
    WifiLanNsd* nsd = static_cast<WifiLanNsd*>(pQueryContext);
    nsd->NotifyDnsServiceUnregistered(Status);
  } catch (...) {
    NEARBY_LOGS(ERROR) << "failed to notify the stop of DNS service instance."
                       << Status;
  }
}

void WifiLanNsd::NotifyDnsServiceUnregistered(DWORD status) {
  if (dns_service_stop_latch_.get() != nullptr) {
    dns_service_stop_status_ = status;
    dns_service_stop_latch_.get()->CountDown();
  }
}

bool WifiLanNsd::StopAdvertising() {
  // Need to use Win32 API to deregister the Dnssd instance
  if (!IsAdvertising()) {
    NEARBY_LOGS(WARNING)
        << "Cannot stop advertising because no advertising is running.";
    return false;
  }

  // Init DNS service instance
  std::string instance_name = absl::StrFormat(
      MDNS_INSTANCE_NAME_FORMAT.data(),
      wifi_lan_service_.GetServiceInfo().GetServiceInfoName(), service_type_);
  int port = wifi_lan_service_.GetServiceInfo().GetServiceAddress().second;
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
      WifiLanNsd::Advertising_StopCompleted;

  dns_service_stop_latch_ = std::make_unique<CountDownLatch>(1);
  DWORD status = DnsServiceDeRegister(&dns_service_register_request_, nullptr);

  if (status != DNS_REQUEST_PENDING) {
    NEARBY_LOGS(ERROR) << "failed to stop mDNS advertising for service id ="
                       << service_id_;
    return false;
  }

  // Wait for stop finish
  dns_service_stop_latch_.get()->Await();
  dns_service_stop_latch_ = nullptr;
  if (dns_service_stop_status_ != 0) {
    NEARBY_LOGS(INFO) << "failed to stop mDNS advertising for service id ="
                      << service_id_;
    return false;
  }

  NEARBY_LOGS(INFO) << "succeeded to stop mDNS advertising for service id ="
                    << service_id_;
  nsd_status_ &= (~NSD_STATUS_ADVERTISING);
  return true;
}

bool WifiLanNsd::StartDiscovery(
    api::WifiLanMedium::DiscoveredServiceCallback callback) {
  if (IsDiscovering()) {
    NEARBY_LOGS(WARNING) << "discovery already running for service id ="
                         << service_id_;
    return false;
  }

  std::string selector =
      absl::StrFormat(MDNS_DEVICE_SELECTOR_FORMAT.data(), service_type_);

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
      device_watcher_.Added({this, &WifiLanNsd::Watcher_DeviceAdded});
  device_watcher_updated_event_token =
      device_watcher_.Updated({this, &WifiLanNsd::Watcher_DeviceUpdated});
  device_watcher_removed_event_token =
      device_watcher_.Removed({this, &WifiLanNsd::Watcher_DeviceRemoved});

  device_watcher_.Start();
  discovered_service_callback_ = std::move(callback);
  nsd_status_ |= NSD_STATUS_DISCOVERING;

  NEARBY_LOGS(INFO) << "started to discovery.";

  return true;
}

bool WifiLanNsd::StopDiscovery() {
  if (!IsDiscovering()) {
    NEARBY_LOGS(WARNING) << "no discovering service to stop.";
    return false;
  }
  // TODO: handle exception
  device_watcher_.Stop();
  device_watcher_.Added(device_watcher_added_event_token);
  device_watcher_.Updated(device_watcher_updated_event_token);
  device_watcher_.Removed(device_watcher_removed_event_token);
  nsd_status_ &= (~NSD_STATUS_DISCOVERING);
  device_watcher_ = nullptr;
  return true;
}

std::pair<std::string, int> WifiLanNsd::GetServiceAddress() {
  if (!IsAdvertising()) {
    // no advertising is running
    NEARBY_LOGS(WARNING) << "no advertising for service id " << service_id_;
    return std::pair<std::string, int>{"", 0};
  }

  return wifi_lan_service_.GetServiceInfo().GetServiceAddress();
}

std::vector<std::string> WifiLanNsd::GetIpAddresses() {
  std::vector<std::string> result{};
  auto host_names = NetworkInformation::GetHostNames();
  for (auto host_name : host_names) {
    if (host_name.IPInformation() != nullptr &&
        host_name.IPInformation().NetworkAdapter() != nullptr) {
      result.push_back(wstring_to_string(host_name.ToString().c_str()));
    }
  }
  return result;
}

fire_and_forget WifiLanNsd::Listener_ConnectionReceived(
    StreamSocketListener listener,
    StreamSocketListenerConnectionReceivedEventArgs const& args) {
  NEARBY_LOGS(INFO) << "got connection message for service " << service_id_;
  StreamSocket stream_socket = args.Socket();

  // Send to callback
  WifiLanSocket socket{stream_socket};
  socket.SetMedium(medium_);
  socket.SetServiceId(service_id_);
  accepted_connection_callback_.accepted_cb(socket, service_id_);
  return fire_and_forget{};
}

NsdServiceInfo WifiLanNsd::GetNsdServiceInformation(
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
  nsd_service_info.SetServiceInfoName(
      InspectableReader::ReadString(inspectable));

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

  std::string ip_address = ipaddresses[0];

  // read IP port
  inspectable = properties.TryLookup(L"System.Devices.Dnssd.PortNumber");
  if (inspectable == nullptr) {
    NEARBY_LOGS(WARNING) << "no IP port information in device information.";
    return nsd_service_info;
  }

  int port = InspectableReader::ReadUint16(inspectable);
  nsd_service_info.SetServiceAddress(ip_address, port);

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

fire_and_forget WifiLanNsd::Watcher_DeviceAdded(DeviceWatcher sender,
                                                DeviceInformation deviceInfo) {
  NEARBY_LOGS(INFO) << "device added for service " << service_id_;

  // need to read IP address and port informaiton from deviceInfo
  NsdServiceInfo nsd_service_info =
      GetNsdServiceInformation(deviceInfo.Properties());

  std::string endpoint =
      nsd_service_info.GetTxtRecord(KEY_ENDPOINT_INFO.data());
  if (endpoint.empty()) {
    return fire_and_forget{};
  }

  std::unique_ptr<WifiLanService> wifi_lan_service =
      std::make_unique<WifiLanService>(nsd_service_info);
  WifiLanService* pwifi_lan_service =
      GetRemoteWifiLanService(endpoint, std::move(wifi_lan_service));
  discovered_service_callback_.service_discovered_cb(*pwifi_lan_service,
                                                     service_id_);
  return fire_and_forget();
}
fire_and_forget WifiLanNsd::Watcher_DeviceUpdated(
    DeviceWatcher sender, DeviceInformationUpdate deviceInfoUpdate) {
  // TODO(b/200421481): discovery servcie callback needs to support device
  // update.
  NEARBY_LOGS(INFO) << "device updated for service " << service_id_;

  return fire_and_forget();
}
fire_and_forget WifiLanNsd::Watcher_DeviceRemoved(
    DeviceWatcher sender, DeviceInformationUpdate deviceInfoUpdate) {
  NEARBY_LOGS(INFO) << "device removed for service " << service_id_;
  // need to read IP address and port informaiton from deviceInfo
  NsdServiceInfo nsd_service_info =
      GetNsdServiceInformation(deviceInfoUpdate.Properties());

  std::string endpoint =
      nsd_service_info.GetTxtRecord(KEY_ENDPOINT_INFO.data());
  if (endpoint.empty()) {
    return fire_and_forget{};
  }
  std::unique_ptr<WifiLanService> wifi_lan_service =
      std::make_unique<WifiLanService>(nsd_service_info);
  WifiLanService* pwifi_lan_service =
      GetRemoteWifiLanService(endpoint, std::move(wifi_lan_service));
  discovered_service_callback_.service_lost_cb(*pwifi_lan_service, service_id_);
  RemoveRemoteWifiLanService(endpoint);
  return fire_and_forget();
}

uint16 WifiLanNsd::GenerateSocketPort(const std::string& service_id) {
  ByteArray service_id_sha = Sha256(service_id, 4);
  char* hash = service_id_sha.data();
  int b1 = hash[0] & 0xff;
  int b2 = hash[1] & 0xff;
  int b3 = hash[2] & 0xff;
  int b4 = hash[3] & 0xff;
  int hashValue = (b1 << 24) | (b2 << 16) | (b3 << 8) | b4;
  return PORT_MIN + (hashValue % PORT_RANGE);
}

WifiLanService* WifiLanNsd::GetRemoteWifiLanService(
    std::string endpoint, std::unique_ptr<WifiLanService> wifi_lan_service) {
  MutexLock lock(&mutex_);

  remote_wifi_lan_services_[endpoint] = std::move(wifi_lan_service);
  return remote_wifi_lan_services_[endpoint].get();
}

void WifiLanNsd::RemoveRemoteWifiLanService(std::string endpoint) {
  MutexLock lock(&mutex_);
  if (remote_wifi_lan_services_.contains(endpoint)) {
    remote_wifi_lan_services_.erase(endpoint);
  }
}

std::string WifiLanNsd::GetServiceIdHash() {
  // Get hashed service id
  ByteArray service_id_sha = Sha256(service_id_, SERVICE_ID_HASH_LENGTH);
  char* sha_data = service_id_sha.data();
  std::string service_id_hash = absl::StrFormat(
      SERVICE_ID_FORMAT.data(), sha_data[0] & 0xFF, sha_data[1] & 0xFF,
      sha_data[2] & 0xFF, sha_data[3] & 0xFF, sha_data[4] & 0xFF,
      sha_data[5] & 0xFF);

  return service_id_hash;
}

}  // namespace windows
}  // namespace nearby
}  // namespace location
