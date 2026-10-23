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

#ifndef CORE_INTERNAL_MEDIUMS_AWDL_AWDL_STUB_H_
#define CORE_INTERNAL_MEDIUMS_AWDL_AWDL_STUB_H_

#include <memory>
#include <string>

#include "connections/implementation/bwu_handler.h"
#include "connections/implementation/endpoint_channel.h"
#include "connections/implementation/mediums/awdl/awdl_interface.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/expected.h"
#include "internal/platform/implementation/psk_info.h"
#include "internal/platform/nsd_service_info.h"
#include "proto/connections_enums.pb.h"

namespace nearby {
namespace connections {

// Stub implementation of Awdl for platforms that do not support AWDL (e.g.
// Windows).
class AwdlStub : public AwdlInterface {
 public:
  AwdlStub() = default;
  ~AwdlStub() override = default;
  AwdlStub(const AwdlStub&) = delete;
  AwdlStub& operator=(const AwdlStub&) = delete;
  AwdlStub(AwdlStub&&) = delete;
  AwdlStub& operator=(AwdlStub&&) = delete;

  bool IsAvailable() const override { return false; }

  ErrorOr<bool> StartAdvertising(const std::string& service_id,
                                 NsdServiceInfo& nsd_service_info) override {
    return {Error(location::nearby::proto::connections::OperationResultCode::
                      MEDIUM_UNAVAILABLE_AWDL_NOT_AVAILABLE)};
  }

  bool StopAdvertising(const std::string& service_id) override { return false; }

  bool IsAdvertising(const std::string& service_id) override { return false; }

  ErrorOr<bool> StartDiscovery(const std::string& service_id,
                               DiscoveredServiceCallback callback) override {
    return {Error(location::nearby::proto::connections::OperationResultCode::
                      MEDIUM_UNAVAILABLE_AWDL_NOT_AVAILABLE)};
  }

  bool StopDiscovery(const std::string& service_id) override { return false; }

  bool IsDiscovering(const std::string& service_id) override { return false; }

  ErrorOr<bool> StartAcceptingConnections(
      const std::string& service_id, const std::string& channel_name,
      AcceptedConnectionCallback callback) override {
    return {Error(location::nearby::proto::connections::OperationResultCode::
                      MEDIUM_UNAVAILABLE_AWDL_NOT_AVAILABLE)};
  }

  ErrorOr<bool> StartAcceptingConnections(
      const std::string& service_id, const std::string& channel_name,
      const api::PskInfo& psk_info,
      AcceptedConnectionCallback callback) override {
    return {Error(location::nearby::proto::connections::OperationResultCode::
                      MEDIUM_UNAVAILABLE_AWDL_NOT_AVAILABLE)};
  }

  bool StopAcceptingConnections(const std::string& service_id) override {
    return false;
  }

  bool IsAcceptingConnections(const std::string& service_id) override {
    return false;
  }

  ErrorOr<std::unique_ptr<EndpointChannel>> Connect(
      const std::string& service_id, const std::string& channel_name,
      const NsdServiceInfo& service_info,
      CancellationFlag* cancellation_flag) override {
    return {Error(location::nearby::proto::connections::OperationResultCode::
                      MEDIUM_UNAVAILABLE_AWDL_NOT_AVAILABLE)};
  }

  ErrorOr<std::unique_ptr<EndpointChannel>> Connect(
      const std::string& service_id, const NsdServiceInfo& service_info,
      const api::PskInfo& psk_info,
      CancellationFlag* cancellation_flag) override {
    return {Error(location::nearby::proto::connections::OperationResultCode::
                      MEDIUM_UNAVAILABLE_AWDL_NOT_AVAILABLE)};
  }

  AwdlCredential GetCredentials(const std::string& service_id) override {
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

#endif  // CORE_INTERNAL_MEDIUMS_AWDL_AWDL_STUB_H_
