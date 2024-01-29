// Copyright 2024 Google LLC
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

#include "connections/dart/nearby_connections_client_state.h"

#include <deque>
#include <memory>
#include <optional>
#include <utility>

#include "absl/synchronization/mutex.h"
#include "third_party/dart_lang/v2/runtime/include/dart_api.h"
#include "connections/c/nc_types.h"
#include "connections/dart/nc_adapter_types.h"

namespace nearby::connections::dart {

NC_INSTANCE NearbyConnectionsClientState::GetOpennedService() const {
  absl::MutexLock lock(&mutex_);
  return opened_instance_;
}

void NearbyConnectionsClientState::SetOpennedService(NC_INSTANCE nc_instance) {
  absl::MutexLock lock(&mutex_);
  opened_instance_ = nc_instance;
}

DiscoveryListenerDart*
NearbyConnectionsClientState::GetDiscoveryListenerDart() {
  absl::MutexLock lock(&mutex_);
  return discovery_listener_dart_.get();
}

void NearbyConnectionsClientState::SetDiscoveryListenerDart(
    std::unique_ptr<DiscoveryListenerDart> discovery_listener_dart) {
  absl::MutexLock lock(&mutex_);
  discovery_listener_dart_ = std::move(discovery_listener_dart);
}

ConnectionListenerDart*
NearbyConnectionsClientState::GetConnectionListenerDart() {
  absl::MutexLock lock(&mutex_);
  return connection_listener_dart_.get();
}

void NearbyConnectionsClientState::SetConnectionListenerDart(
    std::unique_ptr<ConnectionListenerDart> connection_listener_dart) {
  absl::MutexLock lock(&mutex_);
  connection_listener_dart_ = std::move(connection_listener_dart);
}

PayloadListenerDart* NearbyConnectionsClientState::GetPayloadListenerDart() {
  absl::MutexLock lock(&mutex_);
  return payload_listener_dart_.get();
}

void NearbyConnectionsClientState::SetPayloadListenerDart(
    std::unique_ptr<PayloadListenerDart> payload_listener_dart) {
  absl::MutexLock lock(&mutex_);
  payload_listener_dart_ = std::move(payload_listener_dart);
}

std::optional<Dart_Port>
NearbyConnectionsClientState::PopNearbyConnectionsApiPort(
    NearbyConnectionsApi api) {
  absl::MutexLock lock(&mutex_);
  std::deque<Dart_Port>& port_list = nearby_connections_api_ports_[api];
  if (port_list.empty()) {
    return std::nullopt;
  }

  Dart_Port port = port_list.front();
  port_list.pop_front();
  return port;
}

void NearbyConnectionsClientState::PushNearbyConnectionsApiPort(
    NearbyConnectionsApi api, Dart_Port dart_port) {
  absl::MutexLock lock(&mutex_);
  std::deque<Dart_Port>& port_list = nearby_connections_api_ports_[api];
  port_list.push_back(dart_port);
}

void NearbyConnectionsClientState::reset() {
  absl::MutexLock lock(&mutex_);
  opened_instance_ = nullptr;
  nearby_connections_api_ports_.clear();
  discovery_listener_dart_.reset();
  connection_listener_dart_.reset();
  payload_listener_dart_.reset();
}

}  // namespace nearby::connections::dart
