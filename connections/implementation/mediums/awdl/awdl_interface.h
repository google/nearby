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

#ifndef CORE_INTERNAL_MEDIUMS_AWDL_AWDL_INTERFACE_H_
#define CORE_INTERNAL_MEDIUMS_AWDL_AWDL_INTERFACE_H_

#include <memory>
#include <string>

#include "absl/functional/any_invocable.h"
#include "connections/implementation/bwu_handler.h"
#include "connections/implementation/endpoint_channel.h"
#include "internal/platform/awdl.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/expected.h"
#include "internal/platform/implementation/psk_info.h"
#include "internal/platform/nsd_service_info.h"

namespace nearby {
namespace connections {

// Polymorphic interface for the Awdl medium.
class AwdlInterface {
 public:
  using DiscoveredServiceCallback = AwdlMedium::DiscoveredServiceCallback;

  // Callback that is invoked when a new connection is accepted.
  using AcceptedConnectionCallback = absl::AnyInvocable<void(
      const std::string& service_id, std::unique_ptr<EndpointChannel> channel)>;

  struct AwdlCredential {
    std::string service_name;
    std::string service_type;
    std::string password;
  };

  virtual ~AwdlInterface() = default;

  virtual bool IsAvailable() const = 0;

  virtual ErrorOr<bool> StartAdvertising(const std::string& service_id,
                                         NsdServiceInfo& nsd_service_info) = 0;

  virtual bool StopAdvertising(const std::string& service_id) = 0;

  virtual bool IsAdvertising(const std::string& service_id) = 0;

  virtual ErrorOr<bool> StartDiscovery(const std::string& service_id,
                                       DiscoveredServiceCallback callback) = 0;

  virtual bool StopDiscovery(const std::string& service_id) = 0;

  virtual bool IsDiscovering(const std::string& service_id) = 0;

  virtual ErrorOr<bool> StartAcceptingConnections(
      const std::string& service_id, const std::string& channel_name,
      AcceptedConnectionCallback callback) = 0;

  virtual ErrorOr<bool> StartAcceptingConnections(
      const std::string& service_id, const std::string& channel_name,
      const api::PskInfo& psk_info, AcceptedConnectionCallback callback) = 0;

  virtual bool StopAcceptingConnections(const std::string& service_id) = 0;

  virtual bool IsAcceptingConnections(const std::string& service_id) = 0;

  virtual ErrorOr<std::unique_ptr<EndpointChannel>> Connect(
      const std::string& service_id, const std::string& channel_name,
      const NsdServiceInfo& service_info,
      CancellationFlag* cancellation_flag) = 0;

  virtual ErrorOr<std::unique_ptr<EndpointChannel>> Connect(
      const std::string& service_id, const NsdServiceInfo& service_info,
      const api::PskInfo& psk_info, CancellationFlag* cancellation_flag) = 0;

  virtual AwdlCredential GetCredentials(const std::string& service_id) = 0;

  virtual std::unique_ptr<BwuHandler> CreateBwuHandler(
      BwuHandler::IncomingConnectionCallback incoming_connection_callback) = 0;
};

std::unique_ptr<AwdlInterface> CreateAwdl();

}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_MEDIUMS_AWDL_AWDL_INTERFACE_H_
