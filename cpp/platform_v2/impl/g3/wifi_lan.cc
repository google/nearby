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

#include "platform_v2/impl/g3/wifi_lan.h"

#include <memory>
#include <string>

#include "platform_v2/api/wifi_lan.h"
#include "platform_v2/base/logging.h"
#include "platform_v2/base/medium_environment.h"
#include "absl/synchronization/mutex.h"

namespace location {
namespace nearby {
namespace g3 {

InputStream& WifiLanSocket::GetInputStream() {
  absl::MutexLock lock(&mutex_);
  return pipe_.GetInputStream();
}

OutputStream& WifiLanSocket::GetOutputStream() {
  absl::MutexLock lock(&mutex_);
  return pipe_.GetOutputStream();
}

Exception WifiLanSocket::Close() {
  absl::MutexLock lock(&mutex_);
  pipe_.GetOutputStream().Close();
  pipe_.GetInputStream().Close();
  return {Exception::kSuccess};
}

WifiLanService* WifiLanSocket::GetRemoteWifiLanService() {
  absl::MutexLock lock(&mutex_);
  return service_;
}

WifiLanMedium::WifiLanMedium() {
  auto& env = MediumEnvironment::Instance();
  env.RegisterWifiLanMedium(*this);
}

WifiLanMedium::~WifiLanMedium() {
  auto& env = MediumEnvironment::Instance();
  env.UnregisterWifiLanMedium(*this);
}

bool WifiLanMedium::StartAdvertising(
    const std::string& service_id,
    const std::string& wifi_lan_service_info_name) {
  // TODO(edwinwu): Integrate medium_environment.
  // steps:
  // 1. create wifi_lan_service as the parameter to create wifi_lan_socket
  // auto service = std::make_unique<WifiLanService>();
  // auto socket =  std::make_unique<WifiLanSocket>(service);
  // 2. callback for accepting connection; otherwise don't callback if not
  // accepted connection.
  // accepted_connection_callback_.accepted_cb(socket, service_id);
  return true;
}

bool WifiLanMedium::StopAdvertising(const std::string& service_id) {
  // TODO(edwinwu): Integrate medium_environment.
  return true;
}

bool WifiLanMedium::StartDiscovery(const std::string& service_id,
                                   DiscoveredServiceCallback callback) {
  auto& env = MediumEnvironment::Instance();
  NEARBY_LOG(INFO, "G3 StartDiscovery: service_id=%s", service_id.c_str());
  env.UpdateWifiLanMediumForDiscovery(*this, service_, service_id,
                                      std::move(callback), true);
  return true;
}

bool WifiLanMedium::StopDiscovery(const std::string& service_id) {
  auto& env = MediumEnvironment::Instance();
  env.UpdateWifiLanMediumForDiscovery(*this, service_, service_id, {}, false);
  return true;
}

bool WifiLanMedium::StartAcceptingConnections(
    const std::string& service_id, AcceptedConnectionCallback callback) {
  // TODO(edwinwu): Integrate medium_environment.
  // steps:
  auto& env = MediumEnvironment::Instance();
  env.UpdateWifiLanMediumForAcceptedConnection(*this, service_id, callback);
  return true;
}

bool WifiLanMedium::StopAcceptingConnections(const std::string& service_id) {
  // TODO(edwinwu): Integrate medium_environment.
  auto& env = MediumEnvironment::Instance();
  env.UpdateWifiLanMediumForAcceptedConnection(*this, service_id, {});
  return true;
}

std::unique_ptr<api::WifiLanSocket> WifiLanMedium::Connect(
    api::WifiLanService& service, const std::string& service_id) {
  auto socket = std::make_unique<WifiLanSocket>();
  NEARBY_LOG(INFO, "G3 Connect: medium=%p, service_id=%s", this,
             service_id.c_str());
  return socket;
  // TODO(edwinwu): Integrate medium_environment.
  // steps:
  // Request a connection, and block until the socket is provided via the
  // callback.
  // 1. connection = wifi_lan_service.requestConnection_();
  // 2. create wifi_lan_socket with wifi_lan_service and connection
  // return wifi_lan_socket;
}

}  // namespace g3
}  // namespace nearby
}  // namespace location
