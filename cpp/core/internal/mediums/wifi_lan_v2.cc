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

#include "core/internal/mediums/wifi_lan_v2.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/strings/str_format.h"
#include "core/internal/mediums/utils.h"
#include "platform/public/logging.h"
#include "platform/public/mutex_lock.h"

namespace location {
namespace nearby {
namespace connections {

WifiLanV2::~WifiLanV2() {
  // All the AcceptLoopRunnable objects in here should already have gotten an
  // opportunity to shut themselves down cleanly in the calls to
  // StopAcceptingConnections() above.
  accept_loops_runner_.Shutdown();
}

bool WifiLanV2::IsAvailable() const {
  MutexLock lock(&mutex_);
  return IsAvailableLocked();
}

bool WifiLanV2::IsAvailableLocked() const { return medium_.IsValid(); }

bool WifiLanV2::StartAdvertising(const std::string& service_id,
                                 NsdServiceInfo& nsd_service_info) {
  return false;
}

bool WifiLanV2::StopAdvertising(const std::string& service_id) { return false; }

bool WifiLanV2::IsAdvertising(const std::string& service_id) {
  MutexLock lock(&mutex_);
  return IsAdvertisingLocked(service_id);
}

bool WifiLanV2::IsAdvertisingLocked(const std::string& service_id) {
  return advertising_info_.Existed(service_id);
}

bool WifiLanV2::StartDiscovery(const std::string& service_id,
                               DiscoveredServiceCallback callback) {
  MutexLock lock(&mutex_);
  return false;
}

bool WifiLanV2::StopDiscovery(const std::string& service_id) {
  MutexLock lock(&mutex_);
  return false;
}

bool WifiLanV2::IsDiscovering(const std::string& service_id) {
  MutexLock lock(&mutex_);
  return IsDiscoveringLocked(service_id);
}

bool WifiLanV2::IsDiscoveringLocked(const std::string& service_id) {
  return discovering_info_.Existed(service_id);
}

bool WifiLanV2::StartAcceptingConnections(const std::string& service_id,
                                          AcceptedConnectionCallback callback) {
  MutexLock lock(&mutex_);
  return false;
}

bool WifiLanV2::StopAcceptingConnections(const std::string& service_id) {
  MutexLock lock(&mutex_);
  return false;
}

bool WifiLanV2::IsAcceptingConnections(const std::string& service_id) {
  MutexLock lock(&mutex_);
  return IsAcceptingConnectionsLocked(service_id);
}

bool WifiLanV2::IsAcceptingConnectionsLocked(const std::string& service_id) {
  return server_sockets_.find(service_id) != server_sockets_.end();
}

WifiLanSocketV2 WifiLanV2::Connect(const std::string& service_id,
                                   NsdServiceInfo& service_info,
                                   CancellationFlag* cancellation_flag) {
  MutexLock lock(&mutex_);

  return {};
}

WifiLanSocketV2 WifiLanV2::Connect(const std::string& service_id,
                                   const std::string& ip_address, int port,
                                   CancellationFlag* cancellation_flag) {
  MutexLock lock(&mutex_);

  return {};
}

std::pair<std::string, int> WifiLanV2::GetCredentials(
    const std::string& service_id) {
  MutexLock lock(&mutex_);
  const auto& it = server_sockets_.find(service_id);
  if (it == server_sockets_.end()) {
    return std::pair<std::string, int>();
  }
  return std::pair<std::string, int>(it->second.GetIpAddress(),
                                     it->second.GetPort());
}

std::string WifiLanV2::GenerateServiceType(const std::string& service_id) {
  std::string service_id_hash_string;

  const ByteArray service_id_hash = Utils::Sha256Hash(
      service_id, NsdServiceInfo::kTypeFromServiceIdHashLength);
  for (auto byte : std::string(service_id_hash)) {
    absl::StrAppend(&service_id_hash_string, absl::StrFormat("%02X", byte));
  }

  return absl::StrFormat(NsdServiceInfo::kNsdTypeFormat,
                         service_id_hash_string);
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
