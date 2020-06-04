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

#include "core/internal/mediums/wifi_lan.h"

#include "platform/synchronized.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

template <typename Platform>
WifiLan<Platform>::WifiLan()
    : lock_(Platform::createLock()),
      wifi_lan_medium_(Platform::createWifiLanMedium()) {}

template <typename Platform>
bool WifiLan<Platform>::IsAvailable() {
  Synchronized s(lock_.get());

  return !wifi_lan_medium_.isNull();
}

template <typename Platform>
bool WifiLan<Platform>::StartAdvertising(
    absl::string_view service_id,
    absl::string_view wifi_lan_service_info_name) {
  Synchronized s(lock_.get());

  if (!IsAvailable()) {
    return false;
  }

  // TODO(b/149806065): Implements platform wifi-lan medium.
  // wifi_lan_medium_->StartAdvertising(service_id,
  // wifi_lan_service_info_name));

  advertising_info_.service_id.assign(service_id.data());
  return false;
}

template <typename Platform>
void WifiLan<Platform>::StopAdvertising(absl::string_view service_id) {
  Synchronized s(lock_.get());

  if (!IsAdvertising()) {
    return;
  }

  // TODO(b/149806065): Implements platform wifi-lan medium.
  // wifi_lan_medium_->StopAdvertising(advertising_info_.service_id);
  // Reset our bundle of advertising state to mark that we're no longer
  // advertising.
  advertising_info_.service_id.clear();
}

template <typename Platform>
bool WifiLan<Platform>::IsAdvertising() {
  Synchronized s(lock_.get());

  return !advertising_info_.service_id.empty();
}

template <typename Platform>
bool WifiLan<Platform>::StartDiscovery(
    absl::string_view service_id,
    Ptr<DiscoveredServiceCallback> discovered_service_callback) {
  Synchronized s(lock_.get());

  if (discovered_service_callback.isNull() || service_id.empty()) {
    // TODO(b/149806065): logger.atSevere().log("Refusing to start WifiLan
    // discovering because a null parameter was passed in.");
    return false;
  }

  if (IsDiscovering(service_id)) {
    // TODO(b/149806065): logger.atSevere().log("Refusing to start WifiLan
    // discovering because we are already discovering.");
    return false;
  }

  if (!IsAvailable()) {
    // TODO(b/149806065): logger.atSevere().log("Can't start WifiLan discovering
    // because WifiLan isn't available.");
    return false;
  }

  // Avoid leaks.
  ScopedPtr<Ptr<DiscoveredServiceCallbackBridge>>
      scoped_discovered_service_callback_bridge(
          new DiscoveredServiceCallbackBridge(discovered_service_callback));

  // TODO(b/149806065): Implements platform wifi-lan medium.
  // A possible implementation is:
  // wifi_lan_medium_->StartDiscovery(
  //     service_id, Ptr<DiscoveredServiceCallbackBridge>(
  //                     discovered_service_callback_bridge.release()));

  discovering_info_.service_id.assign(service_id.data());
  return false;
}

template <typename Platform>
void WifiLan<Platform>::StopDiscovery(absl::string_view service_id) {
  Synchronized s(lock_.get());

  if (!IsDiscovering(service_id)) {
    // TODO(b/149806065): logger.atDebug().log("Can't turn off WifiLan
    // discovering because we never started discovering.");
    return;
  }

  // TODO(b/149806065): Implements platform wifi-lan medium.
  // wifi_lan_medium_->StopDiscovery(discovering_info_.service_id);
  // Reset our bundle of scanning state to mark that we're no longer scanning.
  discovering_info_.service_id.clear();
}

template <typename Platform>
bool WifiLan<Platform>::IsDiscovering(absl::string_view service_id) {
  Synchronized s(lock_.get());

  return !discovering_info_.service_id.empty();
}

template <typename Platform>
bool WifiLan<Platform>::StartAcceptingConnections(
    absl::string_view service_id,
    Ptr<AcceptedConnectionCallback> accepted_connection_callback) {
  Synchronized s(lock_.get());

  if (accepted_connection_callback.isNull() || service_id.empty()) {
    // TODO(b/149806065): logger.atSevere().log("Refusing to start accepting
    // WifiLan connections because a null parameter was passed in.");
    return false;
  }

  if (IsAcceptingConnections(service_id)) {
    // TODO(b/149806065): logger.atSevere().log("Refusing to start accepting
    // WifiLan connections for %s because another WifiLan service socket is
    // already in-progress.", service_id);
    return false;
  }

  if (!IsAvailable()) {
    // TODO(b/149806065): logger.atSevere().log("Can't start accepting WifiLan
    // connections for %s because WifiLan isn't available.", serviceId);
    return false;
  }

  ScopedPtr<Ptr<WifiLanAcceptedConnectionCallback>>
      scoped_wifi_lan_accepted_connection_callback(
          new WifiLanAcceptedConnectionCallback(
              accepted_connection_callback));

  // TODO(b/149806065): Implements platform wifi-lan medium.
  // A possible implementation is:
  // wifi_lan_medium_->StartAcceptingConnections(
  //     service_id, Ptr<WifiLanAcceptedConnectionCallback>(
  //                     wifi_lan_accepted_connection_callback.release()));

  accepting_connections_info_.service_id.assign(service_id.data());
  return false;
}

template <typename Platform>
void WifiLan<Platform>::StopAcceptingConnections(absl::string_view service_id) {
  Synchronized s(lock_.get());

  if (!IsAcceptingConnections(service_id)) {
    // TODO(b/149806065): logger.atDebug().log("Can't stop accepting WifiLan
    // connections because it was never started.");
    return;
  }

  // TODO(b/149806065): Implements platform wifi-lan medium.);
  // A possible implementation is:
  // wifi_lan_medium_->StopAcceptingConnections(
  //     accepting_connections_info_.service_id);

  // Reset our bundle of accepting connections state to mark that we're no
  // longer accepting connections.
  accepting_connections_info_.service_id.clear();
}

template <typename Platform>
bool WifiLan<Platform>::IsAcceptingConnections(absl::string_view service_id) {
  Synchronized s(lock_.get());

  return !accepting_connections_info_.service_id.empty();
}

template <typename Platform>
Ptr<WifiLanSocket> WifiLan<Platform>::Connect(
    Ptr<WifiLanService> wifi_lan_service, absl::string_view service_id) {
  Synchronized s(lock_.get());

  if (wifi_lan_service.isNull() || service_id.empty()) {
    return Ptr<WifiLanSocket>();
  }

  if (!IsAvailable()) {
    return Ptr<WifiLanSocket>();
  }

  // TODO(b/149806065): Implements platform wifi-lan medium.
  // A possible implementation is:
  // return wifi_lan_medium_->Connect(wifi_lan_service, service_id);
  return Ptr<WifiLanSocket>();
}

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location
