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

#ifndef CORE_INTERNAL_MEDIUMS_WIFI_DIRECT_WIFI_DIRECT_STUB_H_
#define CORE_INTERNAL_MEDIUMS_WIFI_DIRECT_WIFI_DIRECT_STUB_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "connections/implementation/bwu_handler.h"
#include "connections/implementation/endpoint_channel.h"
#include "connections/implementation/mediums/wifi_direct/wifi_direct_interface.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/expected.h"
#include "internal/platform/wifi_credential.h"

namespace nearby {
namespace connections {

// Stub implementation of WifiDirect for platforms that do not support
// Wi-Fi Direct (e.g. Apple).
class WifiDirectStub : public WifiDirectInterface {
 public:
  WifiDirectStub() = default;
  ~WifiDirectStub() override = default;
  WifiDirectStub(const WifiDirectStub&) = delete;
  WifiDirectStub& operator=(const WifiDirectStub&) = delete;
  WifiDirectStub(WifiDirectStub&&) = delete;
  WifiDirectStub& operator=(WifiDirectStub&&) = delete;

  bool IsGOAvailable() const override { return false; }
  bool IsGCAvailable() const override { return false; }

  bool IsGOStarted() override { return false; }
  bool StartWifiDirect() override { return false; }
  bool StopWifiDirect() override { return false; }

  bool IsConnectedToGO() override { return false; }
  bool ConnectWifiDirect(
      const WifiDirectCredentials& wifi_direct_credentials) override {
    return false;
  }
  bool DisconnectWifiDirect() override { return false; }

  bool StartAcceptingConnections(const std::string& service_id,
                                 AcceptedConnectionCallback callback) override {
    return false;
  }
  bool StopAcceptingConnections(const std::string& service_id) override {
    return false;
  }
  bool IsAcceptingConnections(const std::string& service_id) override {
    return false;
  }

  WifiDirectCredentials* GetCredentials(absl::string_view service_id) override {
    return nullptr;
  }

  std::vector<WifiDirectAuthType> GetSupportedWifiDirectAuthTypes()
      const override {
    return {};
  }

  WifiDirectAuthType GetPreferredWifiDirectAuthType() const override {
    return WifiDirectAuthType::WIFI_DIRECT_TYPE_UNKNOWN;
  }

  bool SetPreferredWifiDirectAuthType(WifiDirectAuthType auth_type) override {
    return false;
  }

  std::unique_ptr<BwuHandler> CreateBwuHandler(
      BwuHandler::IncomingConnectionCallback incoming_connection_callback)
      override {
    return nullptr;
  }

  ErrorOr<std::unique_ptr<EndpointChannel>> Connect(
      const std::string& service_id, const std::string& ip_address, int port,
      CancellationFlag* cancellation_flag) override {
    return {Error(location::nearby::proto::connections::OperationResultCode::
                      MEDIUM_UNAVAILABLE_WIFI_DIRECT_NOT_AVAILABLE)};
  }
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_MEDIUMS_WIFI_DIRECT_WIFI_DIRECT_STUB_H_
