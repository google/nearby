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

#ifndef CORE_INTERNAL_MEDIUMS_WIFI_AWARE_WIFI_AWARE_STUB_H_
#define CORE_INTERNAL_MEDIUMS_WIFI_AWARE_WIFI_AWARE_STUB_H_

#include <memory>
#include <string>

#include "absl/strings/string_view.h"
#include "connections/implementation/bwu_handler.h"
#include "connections/implementation/endpoint_channel.h"
#include "connections/implementation/mediums/wifi_aware/wifi_aware_interface.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/expected.h"
#include "internal/platform/implementation/upgrade_address_info.h"
#include "internal/platform/wifi_aware_service_info.h"
#include "proto/connections_enums.pb.h"

namespace nearby {
namespace connections {

// Stub implementation of WifiAware for platforms that do not support
// WifiAware (e.g. Windows).
class WifiAwareStub : public WifiAwareInterface {
 public:
  WifiAwareStub() = default;
  ~WifiAwareStub() override = default;
  WifiAwareStub(const WifiAwareStub&) = delete;
  WifiAwareStub& operator=(const WifiAwareStub&) = delete;
  WifiAwareStub(WifiAwareStub&&) = delete;
  WifiAwareStub& operator=(WifiAwareStub&&) = delete;

  bool IsAvailable() const override { return false; }

  ErrorOr<bool> StartAdvertising(
      const std::string& service_id, absl::string_view channel_name,
      const WifiAwareServiceInfo& wifi_aware_service_info,
      AcceptedConnectionCallback callback) override {
    return {Error(location::nearby::proto::connections::OperationResultCode::
                      MEDIUM_UNAVAILABLE_WIFI_AWARE_NOT_AVAILABLE)};
  }

  bool StopAdvertising(const std::string& service_id) override { return false; }

  bool IsAdvertising(const std::string& service_id) override { return false; }

  ErrorOr<bool> StartDiscovery(const std::string& service_id,
                               DiscoveredServiceCallback callback) override {
    return {Error(location::nearby::proto::connections::OperationResultCode::
                      MEDIUM_UNAVAILABLE_WIFI_AWARE_NOT_AVAILABLE)};
  }

  bool StopDiscovery(const std::string& service_id) override { return false; }

  bool IsDiscovering(const std::string& service_id) override { return false; }

  bool IsPublishing() override { return false; }
  bool StartPublishing() override { return false; }
  bool StopPublishing() override { return false; }

  bool IsSubscribing() override { return false; }
  bool StartSubscribing() override { return false; }
  bool StopSubscribing() override { return false; }

  void SetExpectedPeerId(absl::string_view peer_id) override {}

  ErrorOr<bool> StartAcceptingConnections(
      const std::string& service_id, absl::string_view channel_name,
      AcceptedConnectionCallback callback) override {
    return {Error(location::nearby::proto::connections::OperationResultCode::
                      MEDIUM_UNAVAILABLE_WIFI_AWARE_NOT_AVAILABLE)};
  }

  bool StopAcceptingConnections(const std::string& service_id) override {
    return false;
  }

  bool IsAcceptingConnections(const std::string& service_id) override {
    return false;
  }

  ErrorOr<std::unique_ptr<EndpointChannel>> Connect(
      const std::string& service_id, absl::string_view channel_name,
      const WifiAwareServiceInfo& service_info,
      CancellationFlag* cancellation_flag) override {
    return {Error(location::nearby::proto::connections::OperationResultCode::
                      MEDIUM_UNAVAILABLE_WIFI_AWARE_NOT_AVAILABLE)};
  }

  ErrorOr<std::unique_ptr<EndpointChannel>> Connect(
      const std::string& service_id, const std::string& service_name,
      const ByteArray& service_info, const std::string& passphrase, int port,
      CancellationFlag* cancellation_flag) override {
    return {Error(location::nearby::proto::connections::OperationResultCode::
                      MEDIUM_UNAVAILABLE_WIFI_AWARE_NOT_AVAILABLE)};
  }

  api::UpgradeAddressInfo GetUpgradeAddressCandidates(
      const std::string& service_id) override {
    return {};
  }

  std::unique_ptr<BwuHandler> CreateBwuHandler(
      BwuHandler::IncomingConnectionCallback incoming_connection_callback)
      override {
    return nullptr;
  }
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_MEDIUMS_WIFI_AWARE_WIFI_AWARE_STUB_H_
