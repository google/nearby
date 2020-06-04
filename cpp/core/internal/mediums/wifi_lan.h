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

#ifndef CORE_INTERNAL_MEDIUMS_WIFI_LAN_H_
#define CORE_INTERNAL_MEDIUMS_WIFI_LAN_H_

#include <cstdint>

#include "platform/api/lock.h"
#include "platform/api/wifi_lan.h"
#include "platform/byte_array.h"
#include "platform/port/string.h"
#include "platform/ptr.h"
#include "absl/strings/string_view.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

class DiscoveredServiceCallback {
 public:
  virtual ~DiscoveredServiceCallback() = default;

  virtual void OnServiceDiscovered(Ptr<WifiLanService> wifi_lan_service) = 0;
  virtual void OnServiceLost(Ptr<WifiLanService> wifi_lan_service) = 0;
};

template <typename Platform>
class WifiLan {
 public:
  WifiLan();
  virtual ~WifiLan() = default;

  bool IsAvailable();

  bool StartAdvertising(absl::string_view service_id,
                        absl::string_view wifi_lan_service_info_name);
  void StopAdvertising(absl::string_view service_id);
  bool IsAdvertising();

  bool StartDiscovery(
      absl::string_view service_id,
      Ptr<DiscoveredServiceCallback> discovered_service_callback);
  void StopDiscovery(absl::string_view service_id);
  bool IsDiscovering(absl::string_view service_id);

  class AcceptedConnectionCallback {
   public:
    virtual ~AcceptedConnectionCallback() = default;

    virtual void OnConnectionAccepted(Ptr<WifiLanSocket> socket,
                                      absl::string_view service_id) = 0;
  };

  bool StartAcceptingConnections(
      absl::string_view service_id,
      Ptr<AcceptedConnectionCallback> accepted_connection_callback);
  void StopAcceptingConnections(absl::string_view service_id);
  bool IsAcceptingConnections(absl::string_view service_id);

  Ptr<WifiLanSocket> Connect(Ptr<WifiLanService> wifi_lan_service,
                             absl::string_view service_id);

 private:
  class DiscoveredServiceCallbackBridge
      : public WifiLanMedium::DiscoveredServiceCallback {
   public:
    explicit DiscoveredServiceCallbackBridge(
       Ptr<mediums::DiscoveredServiceCallback> discovered_service_callback)
       : discovered_service_callback_(discovered_service_callback) {}
    ~DiscoveredServiceCallbackBridge() override = default;

    void OnServiceDiscovered(Ptr<WifiLanService> wifi_lan_service) override {
      discovered_service_callback_->OnServiceDiscovered(wifi_lan_service);
    }
    void OnServiceLost(Ptr<WifiLanService> wifi_lan_service) override {
      discovered_service_callback_->OnServiceLost(wifi_lan_service);
    }

   private:
    ScopedPtr<Ptr<mediums::DiscoveredServiceCallback>>
        discovered_service_callback_;
  };

  class WifiLanAcceptedConnectionCallback
      : public WifiLanMedium::AcceptedConnectionCallback {
   public:
    explicit WifiLanAcceptedConnectionCallback(
        Ptr<WifiLan::AcceptedConnectionCallback> accepted_connection_callback)
        : accepted_connection_callback_(accepted_connection_callback) {}
    ~WifiLanAcceptedConnectionCallback() override = default;

    void OnConnectionAccepted(Ptr<WifiLanSocket> wifi_lan_socket,
                              absl::string_view service_id) override {
      accepted_connection_callback_->OnConnectionAccepted(wifi_lan_socket,
                                                          service_id);
    }

   private:
    ScopedPtr<Ptr<WifiLan::AcceptedConnectionCallback>>
        accepted_connection_callback_;
  };

  struct DiscoveringInfo {
    DiscoveringInfo() = default;
    explicit DiscoveringInfo(absl::string_view service_id)
        : service_id(service_id) {}
    ~DiscoveringInfo() = default;

    string service_id;
  };

  struct AdvertisingInfo {
    AdvertisingInfo() = default;
    explicit AdvertisingInfo(absl::string_view service_id)
        : service_id(service_id) {}
    ~AdvertisingInfo() = default;

    string service_id;
  };

  struct AcceptingConnectionsInfo {
    AcceptingConnectionsInfo() = default;
    explicit AcceptingConnectionsInfo(absl::string_view service_id)
        : service_id(service_id) {}
    ~AcceptingConnectionsInfo() = default;

    string service_id;
  };

  // ------------ GENERAL ------------

  ScopedPtr<Ptr<Lock>> lock_;

  // ---------- CORE WIFILAN------------

  // The underlying, per-platform implementation.
  ScopedPtr<Ptr<WifiLanMedium>> wifi_lan_medium_;

  // ------------ DISCOVERY ------------

  // discovering_info_ is not scoped because it's nullable.
  DiscoveringInfo discovering_info_;

  // ------------ ADVERTISING ------------

  // A bundle of state required to start/stop WifiLan service publishing.
  AdvertisingInfo advertising_info_;

  // A bundle of state required to start/stop accepting WifiLan service
  /// connections.
  AcceptingConnectionsInfo accepting_connections_info_;
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location

#include "core/internal/mediums/wifi_lan.cc"

#endif  // CORE_INTERNAL_MEDIUMS_WIFI_LAN_H_
