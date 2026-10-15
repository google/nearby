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

#ifndef CORE_INTERNAL_BWU_HANDLER_H_
#define CORE_INTERNAL_BWU_HANDLER_H_

#include <memory>
#include <string>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/functional/any_invocable.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/endpoint_channel.h"
#include "internal/platform/expected.h"

namespace nearby {
namespace connections {

// Defines the set of methods that need to be implemented to handle the
// per-Medium-specific operations needed to upgrade an EndpointChannel.
class BwuHandler {
 public:
  struct IncomingSocketConnection {
    explicit IncomingSocketConnection(
        absl_nonnull std::unique_ptr<EndpointChannel> channel)
        : channel(std::move(channel)) {}

    absl_nonnull std::unique_ptr<EndpointChannel> channel;
  };

  using IncomingConnectionCallback = absl::AnyInvocable<void(
      ClientProxy* client,
      std::unique_ptr<IncomingSocketConnection> connection)>;

  virtual ~BwuHandler() = default;

  // Called by the Initiator to setup the upgraded medium for this endpoint (if
  // that hasn't already been done), and returns a serialized UpgradePathInfo
  // that can be sent to the Responder.
  // @BwuHandlerThread
  virtual std::string InitializeUpgradedMediumForEndpoint(
      ClientProxy* client, const std::string& service_id,
      const std::string& endpoint_id) = 0;

  // Called to revert any state changed by the Initiator to set up the upgraded
  // medium for an endpoint. Reverts for _all_ services. Use
  // RevertInitiatorState(service_id, endpoint_id) for a more fine-grained
  // approach.
  // @BwuHandlerThread
  virtual void RevertInitiatorState() = 0;

  // Called to revert any state changed by the Initiator to set up the upgraded
  // medium for an endpoint.
  // @BwuHandlerThread
  virtual void RevertInitiatorState(const std::string& service_id,
                                    const std::string& endpoint_id) = 0;

  virtual void RevertResponderState(const std::string& service_id) = 0;

  // Called by the Responder to set up the upgraded medium for this endpoint (if
  // that hasn't already been done) using the UpgradePathInfo sent by the
  // Initiator, and returns a new EndpointChannel for the upgraded medium.
  // @BwuHandlerThread
  virtual ErrorOr<std::unique_ptr<EndpointChannel>>
  CreateUpgradedEndpointChannel(
      ClientProxy* client, const std::string& service_id,
      const std::string& endpoint_id,
      const location::nearby::connections::BandwidthUpgradeNegotiationFrame::
          UpgradePathInfo& upgrade_path_info) = 0;

  // Returns the upgrade medium of the BwuHandler.
  // @BwuHandlerThread
  virtual location::nearby::proto::connections::Medium GetUpgradeMedium()
      const = 0;

  // Answers the host/client role questions BwuManager asks during dynamic role
  // switching for an upgrade medium that has distinct host and client roles
  // (Wi-Fi Direct group owner/client, Wi-Fi Hotspot AP/station, Wi-Fi Aware
  // publisher/subscriber).
  class RoleSwitchPolicy {
   public:
    virtual ~RoleSwitchPolicy() = default;

    // Returns true if a device whose supported roles are `role` can join this
    // medium as a client. BwuManager passes the local device's roles to decide
    // whether a delegated advertiser accepts an UPGRADE_PATH_AVAILABLE frame
    // for this medium from the remote host.
    virtual bool CanBeClient(
        const location::nearby::connections::MediumRole& role) const = 0;

    // Returns true if the local device, whose supported roles are
    // `local_role`, can host this medium for a remote device whose reported
    // roles are `remote_role`. BwuManager uses this to pick a medium to host
    // when the remote device sends UPGRADE_PATH_REQUEST.
    virtual bool CanHost(
        const location::nearby::connections::MediumRole& local_role,
        const location::nearby::connections::MediumRole& remote_role) const = 0;

    // Returns true if the local device should ask the remote device to host
    // this medium (by sending UPGRADE_PATH_REQUEST) instead of hosting it
    // locally, for example because the local OS can only join it as a client.
    //   `remote_medium_role`: the roles reported by the remote device.
    //   `local_os_info` / `remote_os_info`: the OS of the local / remote
    //     device.
    //   `is_incoming_connection`: true if the remote device initiated the
    //     connection, i.e. the local device is the advertiser.
    virtual bool NeedToSwitchRole(
        const location::nearby::connections::MediumRole& remote_medium_role,
        const location::nearby::connections::OsInfo& local_os_info,
        const location::nearby::connections::OsInfo& remote_os_info,
        bool is_incoming_connection) const = 0;
  };

  // Returns this medium's role-switch policy, or nullptr if the medium has no
  // distinct host and client roles. The returned policy is not owned by the
  // caller and lives as long as this handler.
  //
  // Mediums without distinct host and client roles (WIFI_LAN, AWDL, WEB_RTC,
  // and BLUETOOTH) return nullptr. BwuManager never asks them role questions:
  // it never delegates hosting them, never hosts them in response to an
  // UPGRADE_PATH_REQUEST, and a delegated advertiser ignores an
  // UPGRADE_PATH_AVAILABLE frame for them.
  virtual const RoleSwitchPolicy* GetRoleSwitchPolicy() const {
    return nullptr;
  }

  virtual void OnEndpointDisconnect(ClientProxy* client,
                                    const std::string& endpoint_id) = 0;
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_BWU_HANDLER_H_
