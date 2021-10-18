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

#include "platform/impl/windows/wifi_lan.h"

// Windows headers
#include <windows.h>

// Standard C/C++ headers
#include <codecvt>
#include <locale>
#include <string>

// ABSL headers
#include "absl/strings/str_format.h"

// Nearby connections headers
#include "platform/base/cancellation_flag_listener.h"
#include "platform/impl/windows/utils.h"
#include "platform/public/logging.h"
#include "platform/public/mutex_lock.h"

namespace location {
namespace nearby {
namespace windows {

bool WifiLanMedium::StartAcceptingConnections(
    const std::string& service_id, AcceptedConnectionCallback callback) {
  try {
    WifiLanNsd* nsd = GetNsd(service_id, true);
    return nsd->StartAcceptingConnections(callback);
  } catch (...) {
    NEARBY_LOGS(ERROR) << "failed to start accepting connections due to "
                       << GetErrorMessage(std::current_exception());
    return false;
  }
}

bool WifiLanMedium::StopAcceptingConnections(const std::string& service_id) {
  try {
    WifiLanNsd* nsd = GetNsd(service_id);
    if (nsd == nullptr) {
      NEARBY_LOGS(WARNING) << "no running accepting connections.";
      return false;
    }

    if (nsd->StopAcceptingConnections()) {
      if (nsd->IsIdle()) {
        this->RemoveNsd(service_id);
      }
      return true;
    }

    NEARBY_LOGS(ERROR) << "failed to stop accepting connections.";
    return false;
  } catch (...) {
    NEARBY_LOGS(ERROR) << "failed to stop accepting connections due to "
                       << GetErrorMessage(std::current_exception());
    return false;
  }
}

bool WifiLanMedium::StartAdvertising(const std::string& service_id,
                                     const NsdServiceInfo& nsd_service_info) {
  try {
    WifiLanNsd* nsd = GetNsd(service_id);
    if (nsd == nullptr) {
      NEARBY_LOGS(WARNING)
          << "cannot start advertising without accepting connections.";
      return false;
    }
    return nsd->StartAdvertising(nsd_service_info);
  } catch (...) {
    NEARBY_LOGS(ERROR) << "failed to start advertising due to "
                       << GetErrorMessage(std::current_exception());
    return false;
  }
}

bool WifiLanMedium::StopAdvertising(const std::string& service_id) {
  try {
    WifiLanNsd* nsd = GetNsd(service_id);
    if (nsd == nullptr) {
      NEARBY_LOGS(WARNING)
          << "cannot stop advertising without accepting connections.";
      return false;
    }
    if (nsd->StopAdvertising()) {
      if (nsd->IsIdle()) {
        this->RemoveNsd(service_id);
      }
      return true;
    }

    NEARBY_LOGS(ERROR) << "failed to stop advertising.";
    return false;
  } catch (...) {
    NEARBY_LOGS(ERROR) << "failed to stop advertising due to "
                       << GetErrorMessage(std::current_exception());
    return false;
  }
}

// Returns true once the WifiLan discovery has been initiated.
bool WifiLanMedium::StartDiscovery(const std::string& service_id,
                                   DiscoveredServiceCallback callback) {
  try {
    WifiLanNsd* nsd = GetNsd(service_id, true);
    return nsd->StartDiscovery(callback);
  } catch (...) {
    NEARBY_LOGS(ERROR) << "failed to start discovery due to "
                       << GetErrorMessage(std::current_exception());
    return false;
  }
}

// Returns true once WifiLan discovery for service_id is well and truly
// stopped; after this returns, there must be no more invocations of the
// DiscoveredServiceCallback passed in to StartDiscovery() for service_id.
bool WifiLanMedium::StopDiscovery(const std::string& service_id) {
  try {
    WifiLanNsd* nsd = GetNsd(service_id);
    if (nsd == nullptr) {
      NEARBY_LOGS(WARNING) << "no running discovery to stop.";
      return false;
    }

    if (nsd->StopDiscovery()) {
      if (nsd->IsIdle()) {
        this->RemoveNsd(service_id);
      }
      return true;
    }

    NEARBY_LOGS(WARNING) << "failed to stop discovery.";

    return false;
  } catch (...) {
    NEARBY_LOGS(ERROR) << "failed to stop discovery due to "
                       << GetErrorMessage(std::current_exception());
    return false;
  }
}

// Connects to a WifiLan service.
// On success, returns a new WifiLanSocket.
// On error, returns nullptr.
std::unique_ptr<api::WifiLanSocket> WifiLanMedium::Connect(
    api::WifiLanService& wifi_lan_service, const std::string& service_id,
    CancellationFlag* cancellation_flag) {
  try {
    std::string ip_address = wifi_lan_service.GetServiceInfo().GetIPAddress();
    int port = wifi_lan_service.GetServiceInfo().GetPort();
    if (ip_address.empty() || port == 0) {
      NEARBY_LOGS(ERROR) << "no valid service address and port to connect.";
      return nullptr;
    }

    HostName host_name{string_to_wstring(ip_address)};
    winrt::hstring service_name{winrt::to_hstring(port)};

    StreamSocket socket{};

    // setup cancel listener
    if (cancellation_flag != nullptr) {
      if (cancellation_flag->Cancelled()) {
        NEARBY_LOGS(INFO) << "connect has been cancelled: "
                             "service_id="
                          << service_id;
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
          std::make_unique<WifiLanSocket>(&wifi_lan_service, socket);
      wifi_lan_socket->SetServiceId(service_id);
      wifi_lan_socket->SetMedium(this);
      {
        MutexLock lock(&mutex_);
        wifi_lan_sockets_.insert(wifi_lan_socket.get());
      }

      NEARBY_LOGS(INFO) << "connected to remote Wifi LAN service";
      return wifi_lan_socket;
    } catch (...) {
      NEARBY_LOGS(ERROR) << "failed to connect remote service.";
    }

    return nullptr;
  } catch (...) {
    NEARBY_LOGS(ERROR) << "failed to connect due to "
                       << GetErrorMessage(std::current_exception());
    return nullptr;
  }
}

api::WifiLanService* WifiLanMedium::GetRemoteService(
    const std::string& ip_address, int port) {
  try {
    MutexLock lock(&mutex_);

    for (WifiLanSocket* socket : wifi_lan_sockets_) {
      if (socket->GetLocalAddress() == ip_address &&
          socket->GetLocalPort() == port) {
        return socket->GetRemoteWifiLanService();
      }
    }

    return nullptr;
  } catch (...) {
    NEARBY_LOGS(ERROR) << "failed to get remove service due to "
                       << GetErrorMessage(std::current_exception());
    return nullptr;
  }
}

std::pair<std::string, int> WifiLanMedium::GetCredentials(
    const std::string& service_id) {
  try {
    WifiLanNsd* nsd = GetNsd(service_id, true);
    if (nsd == nullptr) {
      // no nsd service
      NEARBY_LOGS(WARNING) << "no service for service id " << service_id;
      return std::pair<std::string, int>{"", 0};
    }

    return nsd->GetCredentials();
  } catch (...) {
    NEARBY_LOGS(ERROR) << "failed to get service address due to "
                       << GetErrorMessage(std::current_exception());
    return {"", 0};
  }
}

void WifiLanMedium::CloseConnection(WifiLanSocket& socket) {
  MutexLock lock(&mutex_);
  if (wifi_lan_sockets_.contains(&socket)) {
    wifi_lan_sockets_.erase(&socket);
  }
}

WifiLanNsd* WifiLanMedium::GetNsd(std::string service_id, bool create) {
  MutexLock lock(&mutex_);

  if (!service_to_nsd_map_.contains(service_id)) {
    if (create) {
      // if no the service id, create a new one
      std::unique_ptr<WifiLanNsd> nsd =
          std::make_unique<WifiLanNsd>(this, service_id);
      service_to_nsd_map_[service_id] = std::move(nsd);
    }
  }

  return service_to_nsd_map_[service_id].get();
}

bool WifiLanMedium::RemoveNsd(std::string service_id) {
  MutexLock lock(&mutex_);

  if (!service_to_nsd_map_.contains(service_id)) {
    return true;
  }
  auto nsd = service_to_nsd_map_.find(service_id);
  if (nsd == service_to_nsd_map_.end() || nsd->second == nullptr ||
      !nsd->second->IsIdle()) {
    return false;
  }

  service_to_nsd_map_.erase(nsd);
  return true;
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
