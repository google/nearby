#include <arpa/inet.h>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <memory>
#include <netinet/in.h>
#include <sdbus-c++/Types.h>
#include <sys/socket.h>

#include <sdbus-c++/Error.h>
#include <sdbus-c++/IConnection.h>

#include "absl/strings/substitute.h"
#include "internal/platform/implementation/linux/avahi.h"
#include "internal/platform/implementation/linux/dbus.h"
#include "internal/platform/implementation/linux/wifi_lan.h"
#include "internal/platform/implementation/linux/wifi_lan_server_socket.h"
#include "internal/platform/implementation/linux/wifi_lan_socket.h"
#include "internal/platform/implementation/linux/wifi_medium.h"
#include "internal/platform/implementation/wifi_lan.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace linux {
WifiLanMedium::WifiLanMedium(sdbus::IConnection &system_bus)
    : system_bus_(system_bus),
      network_manager_(std::make_shared<linux::NetworkManager>(system_bus)),
      avahi_(std::make_shared<avahi::Server>(system_bus)),
      entry_group_(nullptr) {}

WifiLanMedium::~WifiLanMedium() {
  if (entry_group_ != nullptr) {
    entry_group_->Free();
  }
}

bool WifiLanMedium::IsNetworkConnected() const {
  auto state = network_manager_->getState();
  return state >= 50; // NM_STATE_CONNECTED_LOCAL
}

bool WifiLanMedium::StartAdvertising(const NsdServiceInfo &nsd_service_info) {
  if (entry_group_ == nullptr) {
    try {
      auto object_path = avahi_->EntryGroupNew();
      entry_group_ =
          std::make_unique<avahi::EntryGroup>(system_bus_, object_path);
      NEARBY_LOGS(VERBOSE) << __func__ << "Created a new entry group at "
                           << entry_group_->getObjectPath();
    } catch (const sdbus::Error &e) {
      DBUS_LOG_METHOD_CALL_ERROR(avahi_, "EntryGroupNew", e);
      NEARBY_LOGS(ERROR) << __func__ << ": Could not create a new entry group.";
      return false;
    }
  }

  if (advertising_) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Cannot advertise while we are already advertising";
    return false;
  }

  auto txt_records_map = nsd_service_info.GetTxtRecords();
  std::vector<std::vector<std::uint8_t>> txt_records(txt_records_map.size());
  std::size_t i = 0;

  for (auto [key, value] : nsd_service_info.GetTxtRecords()) {
    std::string entry = absl::Substitute("$0=$1", key, value);
    txt_records[i++] = std::vector<std::uint8_t>(entry.begin(), entry.end());
  }

  try {
    entry_group_->AddService(
        -1, // AVAHI_IF_UNSPEC
        -1, // AVAHI_PROTO_UNSPED
        0, nsd_service_info.GetServiceName(), nsd_service_info.GetServiceType(),
        std::string(), std::string(), nsd_service_info.GetPort(), txt_records);
    entry_group_->Commit();
  } catch (const sdbus::Error &e) {
    NEARBY_LOGS(ERROR) << __func__ << ": Got error '" << e.getName()
                       << "' with message '" << e.getMessage()
                       << "' while adding service";
    return false;
  }

  advertising_ = true;
  return true;
}

bool WifiLanMedium::StopAdvertising(const NsdServiceInfo &nsd_service_info) {
  if (!advertising_) {
    NEARBY_LOGS(ERROR) << __func__ << ": Advertising is already stopped.";
    return false;
  }
  if (entry_group_ == nullptr) {
    NEARBY_LOGS(ERROR) << __func__ << ": No entry group registered.";
    return false;
  }

  try {
    if (entry_group_->IsEmpty()) {
      NEARBY_LOGS(ERROR)
          << __func__ << ": Cannot stop advertising on an empty entry group.";
      return false;
    }
    entry_group_->Reset();
    entry_group_->Commit();
  } catch (const sdbus::Error &e) {
    NEARBY_LOGS(ERROR) << __func__ << ": Got error '" << e.getName()
                       << "' with message '" << e.getMessage()
                       << "' while removing service";
    return false;
  }

  advertising_ = false;
  return true;
}

bool WifiLanMedium::StartDiscovery(
    const std::string &service_type,
    api::WifiLanMedium::DiscoveredServiceCallback callback) {
  if (service_browsers_.count(service_type) != 0) {
    auto &object = service_browsers_[service_type];
    NEARBY_LOGS(ERROR) << __func__ << ": A service browser for service type "
                       << service_type << " already exists at "
                       << object->getObjectPath();
    return false;
  }

  try {
    sdbus::ObjectPath browser_object_path =
        avahi_->ServiceBrowserPrepare(-1, // AVAHI_IF_UNSPEC
                                      -1, // AVAHI_PROTO_UNSPED
                                      service_type, std::string(), 0);
    NEARBY_LOGS(VERBOSE)
        << __func__
        << ": Created a new org.freedesktop.Avahi.ServiceBrowser object at "
        << browser_object_path;
    service_browsers_.emplace(
        service_type,
        std::make_unique<avahi::ServiceBrowser>(
            system_bus_, browser_object_path, std::move(callback)));
  } catch (const sdbus::Error &e) {
    DBUS_LOG_METHOD_CALL_ERROR(avahi_, "ServiceBrowserPrepare", e);
    return false;
  }

  auto &browser = service_browsers_[service_type];
  try {
    NEARBY_LOGS(VERBOSE) << __func__ << ": Starting service discovery for "
                         << browser->getObjectPath();
    browser->Start();
  } catch (const sdbus::Error &e) {
    DBUS_LOG_METHOD_CALL_ERROR(browser, "Start", e);
    return false;
  }

  return true;
}

bool WifiLanMedium::StopDiscovery(const std::string &service_type) {
  if (service_browsers_.count(service_type) == 0) {
    NEARBY_LOGS(ERROR) << __func__ << ": Service type " << service_type
                       << " has not been registered for discovery";
    return false;
  }

  auto &browser = service_browsers_[service_type];
  try {
    browser->Free();
  } catch (const sdbus::Error &e) {
    DBUS_LOG_METHOD_CALL_ERROR(browser, "Free", e);
    return false;
  }

  service_browsers_.erase(service_type);

  return true;
}

std::unique_ptr<api::WifiLanSocket>
WifiLanMedium::ConnectToService(const std::string &ip_address, int port,
                                CancellationFlag *cancellation_flag) {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Error opening socket: " << std::strerror(errno);
    return nullptr;
  }

  NEARBY_LOGS(VERBOSE) << __func__ << ": Connecting to " << ip_address << ":"
                       << port;
  struct sockaddr_in addr;
  addr.sin_addr.s_addr = inet_addr(ip_address.c_str());
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);

  auto ret =
      connect(sock, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr));
  if (ret < 0) {
    NEARBY_LOGS(ERROR) << __func__ << ": Error connecting to socket: "
                       << std::strerror(errno);
    return nullptr;
  }

  sdbus::UnixFd fd(sock);
  return std::make_unique<WifiLanSocket>(std::move(fd));
}

std::unique_ptr<api::WifiLanServerSocket>
WifiLanMedium::ListenForService(int port) {
  auto sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Error opening socket: " << std::strerror(errno);
    return nullptr;
  }

  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port);

  auto ret =
      bind(sock, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr));
  if (ret < 0) {
    NEARBY_LOGS(ERROR) << __func__
                       << ": Error binding to socket: " << std::strerror(errno);
    return nullptr;
  }

  ret = listen(sock, 0);
  if (ret < 0) {
    NEARBY_LOGS(ERROR) << __func__ << ": Error listening on socket: "
                       << std::strerror(errno);
    return nullptr;
  }

  NEARBY_LOGS(VERBOSE) << __func__ << "Listening for services on port " << port;

  return std::make_unique<WifiLanServerSocket>(sock, network_manager_,
                                               system_bus_);
}

absl::optional<std::pair<std::int32_t, std::int32_t>> GetDynamicPortRange() {
  return absl::nullopt;
}

} // namespace linux
} // namespace nearby
