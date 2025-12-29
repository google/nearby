// Copyright 2023 Google LLC
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

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <memory>
#include <utility>

#include <sdbus-c++/Error.h>
#include <sdbus-c++/IConnection.h>
#include <sdbus-c++/Types.h>

#include "absl/strings/substitute.h"
#include "internal/platform/implementation/linux/avahi.h"
#include "internal/platform/implementation/linux/dbus.h"
#include "internal/platform/implementation/linux/tcp_server_socket.h"
#include "internal/platform/implementation/linux/wifi_lan.h"
#include "internal/platform/implementation/linux/wifi_lan_server_socket.h"
#include "internal/platform/implementation/linux/wifi_lan_socket.h"
#include "internal/platform/implementation/wifi_lan.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace linux {
WifiLanMedium::WifiLanMedium(
    std::shared_ptr<networkmanager::NetworkManager> network_manager)
    : system_bus_(network_manager->GetConnection()),
      network_manager_(std::move(network_manager)),
      avahi_(std::make_shared<avahi::Server>(*system_bus_)) {}

bool WifiLanMedium::IsNetworkConnected() const {
  auto state = network_manager_->getState();
  return state == networkmanager::NetworkManager::kNMStateConnectedLocal ||
         state == networkmanager::NetworkManager::kNMStateConnectedSite ||
         state == networkmanager::NetworkManager::kNMStateConnectedGlobal;
}

std::optional<std::pair<std::string, std::string>> entry_group_key(
    const NsdServiceInfo &nsd_service_info) {
  auto name = nsd_service_info.GetServiceName();
  if (name.empty()) {
    LOG(ERROR) << __func__ << ": service name cannot be empty";
    return std::nullopt;
  }

  auto type = nsd_service_info.GetServiceType();
  if (type.empty()) {
    LOG(ERROR) << __func__ << ": service type cannot be empty";
    return std::nullopt;
  }

  return std::make_pair(std::move(name), std::move(type));
}

bool WifiLanMedium::StartAdvertising(const NsdServiceInfo &nsd_service_info) {
  auto key = entry_group_key(nsd_service_info);
  if (!key.has_value()) {
    return false;
  }

  {
    absl::ReaderMutexLock l(&entry_groups_mutex_);
    if (entry_groups_.count(*key) == 1) {
      LOG(ERROR) << __func__
                         << ": advertising is already active for this service";
      return false;
    }
  }

  auto txt_records_map = nsd_service_info.GetTxtRecords();
  std::vector<std::vector<std::uint8_t>> txt_records(txt_records_map.size());
  std::size_t i = 0;

  for (auto [key, value] : nsd_service_info.GetTxtRecords()) {
    std::string entry = absl::Substitute("$0=$1", key, value);
    txt_records[i++] = std::vector<std::uint8_t>(entry.begin(), entry.end());
  }

  sdbus::ObjectPath entry_group_path;
  try {
    entry_group_path = avahi_->EntryGroupNew();
  } catch (const sdbus::Error &e) {
    DBUS_LOG_METHOD_CALL_ERROR(avahi_, "EntryGroupNew", e);
    return false;
  }

  auto entry_group =
      std::make_unique<avahi::EntryGroup>(*system_bus_, entry_group_path);

  try {
    entry_group->AddService(
        -1,  // AVAHI_IF_UNSPEC
        -1,  // AVAHI_PROTO_UNSPED
        0, nsd_service_info.GetServiceName(), nsd_service_info.GetServiceType(),
        std::string(), std::string(), nsd_service_info.GetPort(), txt_records);
    entry_group->Commit();
  } catch (const sdbus::Error &e) {
    LOG(ERROR) << __func__ << ": Got error '" << e.getName()
                       << "' with message '" << e.getMessage()
                       << "' while adding service";
    return false;
  }

  absl::MutexLock l(&entry_groups_mutex_);
  entry_groups_.insert({*key, std::move(entry_group)});

  return true;
}

bool WifiLanMedium::StopAdvertising(const NsdServiceInfo &nsd_service_info) {
  auto key = entry_group_key(nsd_service_info);
  if (!key.has_value()) {
    return false;
  }

  absl::MutexLock l(&entry_groups_mutex_);
  if (entry_groups_.count(*key) == 0) {
    LOG(ERROR) << __func__
                       << ": Advertising is already inactive for this service.";
    return false;
  }

  entry_groups_.erase(*key);
  return true;
}

bool WifiLanMedium::StartDiscovery(
    const std::string &service_type,
    api::WifiLanMedium::DiscoveredServiceCallback callback) {
  {
    absl::ReaderMutexLock l(&service_browsers_mutex_);
    if (service_browsers_.count(service_type) != 0) {
      auto &object = service_browsers_[service_type];
      LOG(ERROR) << __func__ << ": A service browser for service type "
                         << service_type << " already exists at "
                         << object->getObjectPath();
      return false;
    }
  }

  try {
    sdbus::ObjectPath browser_object_path =
        avahi_->ServiceBrowserPrepare(-1,  // AVAHI_IF_UNSPEC
                                      -1,  // AVAHI_PROTO_UNSPED
                                      service_type, std::string(), 0);
    LOG(INFO)
        << __func__
        << ": Created a new org.freedesktop.Avahi.ServiceBrowser object at "
        << browser_object_path;

    absl::MutexLock l(&service_browsers_mutex_);
    service_browsers_.emplace(
        service_type,
        std::make_unique<avahi::ServiceBrowser>(
            *system_bus_, browser_object_path, std::move(callback), avahi_));
  } catch (const sdbus::Error &e) {
    DBUS_LOG_METHOD_CALL_ERROR(avahi_, "ServiceBrowserPrepare", e);
    return false;
  }

  service_browsers_mutex_.ReaderLock();
  auto &browser = service_browsers_[service_type];
  service_browsers_mutex_.ReaderUnlock();

  try {
    LOG(INFO) << __func__ << ": Starting service discovery for "
                         << browser->getObjectPath();
    browser->Start();
  } catch (const sdbus::Error &e) {
    DBUS_LOG_METHOD_CALL_ERROR(browser, "Start", e);
    return false;
  }

  return true;
}

bool WifiLanMedium::StopDiscovery(const std::string &service_type) {
  absl::MutexLock l(&service_browsers_mutex_);

  if (service_browsers_.count(service_type) == 0) {
    LOG(ERROR) << __func__ << ": Service type " << service_type
                       << " has not been registered for discovery";
    return false;
  }
  service_browsers_.erase(service_type);

  return true;
}

std::unique_ptr<api::WifiLanSocket> WifiLanMedium::ConnectToService(
    const std::string &ip_address, int port,
    CancellationFlag *cancellation_flag) {
  auto socket = TCPSocket::Connect(ip_address, port);
  if (!socket.has_value()) return nullptr;
  return std::make_unique<WifiLanSocket>(*socket);
}

std::unique_ptr<api::WifiLanServerSocket> WifiLanMedium::ListenForService(
    int port) {
  auto socket = TCPServerSocket::Listen(std::nullopt, port);
  if (!socket.has_value()) return nullptr;

  return std::make_unique<WifiLanServerSocket>(std::move(*socket),
                                               network_manager_);
}

absl::optional<std::pair<std::int32_t, std::int32_t>> GetDynamicPortRange() {
  return absl::nullopt;
}

}  // namespace linux
}  // namespace nearby
