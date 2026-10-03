// Copyright 2026 Google LLC
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

#ifndef CORE_INTERNAL_MEDIUMS_WIFI_DIRECT_WIFI_DIRECT_INTERFACE_H_
#define CORE_INTERNAL_MEDIUMS_WIFI_DIRECT_WIFI_DIRECT_INTERFACE_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "connections/implementation/bwu_handler.h"
#include "connections/implementation/endpoint_channel.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/expected.h"
#include "internal/platform/wifi_credential.h"

namespace nearby {
namespace connections {

// Polymorphic interface for the WifiDirect medium.
class WifiDirectInterface {
 public:
  using AcceptedConnectionCallback = absl::AnyInvocable<void(
      const std::string& service_id, std::unique_ptr<EndpointChannel> channel)>;
  using WifiDirectAuthType =
      ::location::nearby::proto::connections::WifiDirectAuthType;

  virtual ~WifiDirectInterface() = default;

  virtual bool IsGOAvailable() const = 0;
  virtual bool IsGCAvailable() const = 0;

  virtual bool IsGOStarted() = 0;
  virtual bool StartWifiDirect() = 0;
  virtual bool StopWifiDirect() = 0;

  virtual bool IsConnectedToGO() = 0;
  virtual bool ConnectWifiDirect(
      const WifiDirectCredentials& wifi_direct_credentials) = 0;
  virtual bool DisconnectWifiDirect() = 0;

  virtual bool StartAcceptingConnections(
      const std::string& service_id, AcceptedConnectionCallback callback) = 0;
  virtual bool StopAcceptingConnections(const std::string& service_id) = 0;
  virtual bool IsAcceptingConnections(const std::string& service_id) = 0;

  virtual ErrorOr<std::unique_ptr<EndpointChannel>> Connect(
      const std::string& service_id, const std::string& ip_address, int port,
      CancellationFlag* cancellation_flag) = 0;

  virtual WifiDirectCredentials* GetCredentials(
      absl::string_view service_id) = 0;

  virtual std::vector<WifiDirectAuthType> GetSupportedWifiDirectAuthTypes()
      const = 0;

  virtual WifiDirectAuthType GetPreferredWifiDirectAuthType() const = 0;

  virtual bool SetPreferredWifiDirectAuthType(WifiDirectAuthType auth_type) = 0;

  virtual std::unique_ptr<BwuHandler> CreateBwuHandler(
      BwuHandler::IncomingConnectionCallback incoming_connection_callback) = 0;
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_MEDIUMS_WIFI_DIRECT_WIFI_DIRECT_INTERFACE_H_
