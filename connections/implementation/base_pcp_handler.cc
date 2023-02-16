// Copyright 2021 Google LLC
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

#include "connections/implementation/base_pcp_handler.h"

#include <algorithm>
#include <cassert>
#include <cinttypes>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "securegcm/d2d_connection_context_v1.h"
#include "securegcm/ukey2_handshake.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/escaping.h"
#include "absl/types/span.h"
#include "connections/advertising_options.h"
#include "connections/connection_options.h"
#include "connections/implementation/mediums/utils.h"
#include "connections/implementation/offline_frames.h"
#include "connections/implementation/proto/offline_wire_formats.pb.h"
#include "connections/medium_selector.h"
#include "internal/platform/base64_utils.h"
#include "internal/platform/bluetooth_utils.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace connections {

using ::location::nearby::connections::ConnectionRequestFrame;
using ::location::nearby::connections::ConnectionResponseFrame;
using ::location::nearby::connections::MediumMetadata;
using ::location::nearby::connections::OfflineFrame;
using ::location::nearby::connections::V1Frame;
using ::securegcm::UKey2Handshake;

constexpr absl::Duration BasePcpHandler::kConnectionRequestReadTimeout;
constexpr absl::Duration BasePcpHandler::kRejectedConnectionCloseDelay;

BasePcpHandler::BasePcpHandler(Mediums* mediums,
                               EndpointManager* endpoint_manager,
                               EndpointChannelManager* channel_manager,
                               BwuManager* bwu_manager, Pcp pcp)
    : mediums_(mediums),
      endpoint_manager_(endpoint_manager),
      channel_manager_(channel_manager),
      pcp_(pcp),
      bwu_manager_(bwu_manager) {}

BasePcpHandler::~BasePcpHandler() {
  NEARBY_LOGS(INFO) << "Initiating shutdown of BasePcpHandler("
                    << strategy_.GetName() << ")";
  DisconnectFromEndpointManager();
  // Stop all the ongoing Runnables (as gracefully as possible).
  NEARBY_LOGS(INFO) << "BasePcpHandler(" << strategy_.GetName()
                    << ") is bringing down executors.";
  serial_executor_.Shutdown();
  alarm_executor_.Shutdown();
  NEARBY_LOGS(INFO) << "BasePcpHandler(" << strategy_.GetName()
                    << ") has shut down.";
}

void BasePcpHandler::DisconnectFromEndpointManager() {
  if (stop_.Set(true)) return;
  NEARBY_LOGS(INFO) << "BasePcpHandler(" << strategy_.GetName()
                    << ") unregister from EPM.";
  // Unregister ourselves from EPM message dispatcher.
  endpoint_manager_->UnregisterFrameProcessor(V1Frame::CONNECTION_RESPONSE,
                                              this);
}

Status BasePcpHandler::StartAdvertising(
    ClientProxy* client, const std::string& service_id,
    const AdvertisingOptions& advertising_options,
    const ConnectionRequestInfo& info) {
  Future<Status> response;

  AdvertisingOptions compatible_advertising_options =
      advertising_options.CompatibleOptions();
  StripOutUnavailableMediums(compatible_advertising_options);
  NEARBY_LOGS(INFO) << "StartAdvertising with supported mediums: "
                    << GetStringValueOfSupportedMediums(
                           compatible_advertising_options);

  RunOnPcpHandlerThread(
      "start-advertising",
      [this, client, &service_id, &info, &compatible_advertising_options,
       &response]() RUN_ON_PCP_HANDLER_THREAD() {
        // The endpoint id inside of the advertisement is different to high
        // visibility and low visibility mode. In order to decide if client
        // should grab the high visibility or low visibility id, it needs to
        // tell client which one right now, before
        // client#StartedAdvertising.
        if (ShouldEnterHighVisibilityMode(compatible_advertising_options)) {
          client->EnterHighVisibilityMode();
        }

        auto result = StartAdvertisingImpl(
            client, service_id, client->GetLocalEndpointId(),
            info.endpoint_info, compatible_advertising_options);
        if (!result.status.Ok()) {
          client->ExitHighVisibilityMode();
          response.Set(result.status);
          return;
        }

        // Now that we've succeeded, mark the client as advertising.
        // Save the advertising options for local reference in later process
        // like upgrading bandwidth.
        advertising_listener_ = info.listener;
        client->StartedAdvertising(service_id, GetStrategy(), info.listener,
                                   absl::MakeSpan(result.mediums),
                                   compatible_advertising_options);
        response.Set({Status::kSuccess});
      });
  return WaitForResult(absl::StrCat("StartAdvertising(", service_id, ")"),
                       client->GetClientId(), &response);
}

void BasePcpHandler::StopAdvertising(ClientProxy* client) {
  NEARBY_LOGS(INFO) << "StopAdvertising local_endpoint_id="
                    << client->GetLocalEndpointId();
  CountDownLatch latch(1);
  RunOnPcpHandlerThread("stop-advertising",
                        [this, client, &latch]() RUN_ON_PCP_HANDLER_THREAD() {
                          StopAdvertisingImpl(client);
                          client->StoppedAdvertising();
                          latch.CountDown();
                        });
  WaitForLatch("StopAdvertising", &latch);
}

std::string BasePcpHandler::GetStringValueOfSupportedMediums(
    const ConnectionOptions& connection_options) const {
  std::ostringstream result;
  OptionsAllowed(connection_options.allowed, result);
  return result.str();
}

std::string BasePcpHandler::GetStringValueOfSupportedMediums(
    const AdvertisingOptions& advertising_options) const {
  std::ostringstream result;
  OptionsAllowed(advertising_options.allowed, result);
  return result.str();
}

std::string BasePcpHandler::GetStringValueOfSupportedMediums(
    const DiscoveryOptions& discovery_options) const {
  std::ostringstream result;
  OptionsAllowed(discovery_options.allowed, result);
  return result.str();
}

void BasePcpHandler::OptionsAllowed(const BooleanMediumSelector& allowed,
                                    std::ostringstream& result) const {
  result << "{ ";
  if (allowed.bluetooth) {
    result << location::nearby::proto::connections::Medium_Name(
                  Medium::BLUETOOTH)
           << " ";
  }
  if (allowed.ble) {
    result << location::nearby::proto::connections::Medium_Name(Medium::BLE)
           << " ";
  }
  if (allowed.web_rtc) {
    result << location::nearby::proto::connections::Medium_Name(Medium::WEB_RTC)
           << " ";
  }
  if (allowed.wifi_lan) {
    result << location::nearby::proto::connections::Medium_Name(
                  Medium::WIFI_LAN)
           << " ";
  }
  if (allowed.wifi_hotspot) {
    result << location::nearby::proto::connections::Medium_Name(
                  Medium::WIFI_HOTSPOT)
           << " ";
  }
  if (allowed.wifi_direct) {
    result << location::nearby::proto::connections::Medium_Name(
                  Medium::WIFI_DIRECT)
           << " ";
  }
  result << "}";
}

bool BasePcpHandler::ShouldEnterHighVisibilityMode(
    const AdvertisingOptions& advertising_options) {
  return !advertising_options.low_power &&
         advertising_options.allowed.bluetooth;
}

BooleanMediumSelector BasePcpHandler::ComputeIntersectionOfSupportedMediums(
    const PendingConnectionInfo& connection_info) {
  absl::flat_hash_set<Medium> intersection;
  auto their_mediums = connection_info.supported_mediums;

  // If no supported mediums were set, use the default upgrade medium.
  if (their_mediums.empty()) {
    their_mediums.push_back(GetDefaultUpgradeMedium());
  }
  // TODO(b/268243340): Add Supported Medium field to ConnectionResponseFrame
  if (connection_info.is_incoming) {
    for (auto medium : their_mediums) {
      NEARBY_LOGS(INFO) << "Their supported medium name: "
                        << location::nearby::proto::connections::Medium_Name(
                               medium);
    }
  } else {
    NEARBY_LOGS(INFO)
        << "Current ConnectionResponseFrame from host has no Supported Mediums "
           "field, so use calculated default medium instead.";
  }

  for (Medium my_medium : GetConnectionMediumsByPriority()) {
    NEARBY_LOGS(INFO) << "Our supported medium name: "
                      << location::nearby::proto::connections::Medium_Name(
                             my_medium);
    if (std::find(their_mediums.begin(), their_mediums.end(), my_medium) !=
        their_mediums.end()) {
      // We use advertising options as a proxy to whether or not the local
      // client does want to enable a WebRTC upgrade.
      if (my_medium == location::nearby::proto::connections::Medium::WEB_RTC) {
        AdvertisingOptions advertising_options =
            connection_info.client->GetAdvertisingOptions();

        if (!advertising_options.enable_webrtc_listening &&
            !advertising_options.allowed.web_rtc) {
          // The local client does not allow WebRTC for listening or upgrades,
          // ignore.
          continue;
        }
      }

      intersection.emplace(my_medium);
    }
  }

  // Not using designated initializers here since the VS C++ compiler errors
  // out indicating that MediumSelector<bool> is not an aggregate
  BooleanMediumSelector mediumSelector{};
  mediumSelector.bluetooth = intersection.contains(Medium::BLUETOOTH);
  mediumSelector.ble = intersection.contains(Medium::BLE);
  mediumSelector.web_rtc = intersection.contains(Medium::WEB_RTC);
  mediumSelector.wifi_lan = intersection.contains(Medium::WIFI_LAN);
  mediumSelector.wifi_hotspot = intersection.contains(Medium::WIFI_HOTSPOT);
  mediumSelector.wifi_direct = intersection.contains(Medium::WIFI_DIRECT);
  return mediumSelector;
}

Status BasePcpHandler::StartDiscovery(ClientProxy* client,
                                      const std::string& service_id,
                                      const DiscoveryOptions& discovery_options,
                                      const DiscoveryListener& listener) {
  Future<Status> response;
  DiscoveryOptions stripped_discovery_options = discovery_options;
  StripOutUnavailableMediums(stripped_discovery_options);
  NEARBY_LOGS(INFO) << "StartDiscovery with supported mediums:"
                    << GetStringValueOfSupportedMediums(
                           stripped_discovery_options);
  RunOnPcpHandlerThread(
      "start-discovery", [this, client, service_id, stripped_discovery_options,
                          &listener, &response]() RUN_ON_PCP_HANDLER_THREAD() {
        // Ask the implementation to attempt to start discovery.
        auto result =
            StartDiscoveryImpl(client, service_id, stripped_discovery_options);
        if (!result.status.Ok()) {
          response.Set(result.status);
          return;
        }

        // Now that we've succeeded, mark the client as discovering and clear
        // out any old endpoints we had discovered.
        discovered_endpoints_.clear();
        client->StartedDiscovery(service_id, GetStrategy(), listener,
                                 absl::MakeSpan(result.mediums),
                                 stripped_discovery_options);
        response.Set({Status::kSuccess});
      });
  return WaitForResult(absl::StrCat("StartDiscovery(", service_id, ")"),
                       client->GetClientId(), &response);
}

void BasePcpHandler::StopDiscovery(ClientProxy* client) {
  CountDownLatch latch(1);
  RunOnPcpHandlerThread("stop-discovery",
                        [this, client, &latch]() RUN_ON_PCP_HANDLER_THREAD() {
                          StopDiscoveryImpl(client);
                          client->StoppedDiscovery();
                          latch.CountDown();
                        });

  WaitForLatch("StopDiscovery", &latch);
}

void BasePcpHandler::InjectEndpoint(
    ClientProxy* client, const std::string& service_id,
    const OutOfBandConnectionMetadata& metadata) {
  CountDownLatch latch(1);
  RunOnPcpHandlerThread("inject-endpoint",
                        [this, client, service_id, metadata, &latch]()
                            RUN_ON_PCP_HANDLER_THREAD() {
                              InjectEndpointImpl(client, service_id, metadata);
                              latch.CountDown();
                            });

  WaitForLatch(absl::StrCat("InjectEndpoint(", service_id, ")"), &latch);
}

void BasePcpHandler::WaitForLatch(const std::string& method_name,
                                  CountDownLatch* latch) {
  Exception await_exception = latch->Await();
  if (!await_exception.Ok()) {
    if (await_exception.Raised(Exception::kTimeout)) {
      NEARBY_LOGS(INFO) << "Blocked in " << method_name;
    }
  }
}

Status BasePcpHandler::WaitForResult(const std::string& method_name,
                                     std::int64_t client_id,
                                     Future<Status>* future) {
  if (!future) {
    NEARBY_LOGS(INFO) << "No future to wait for; return with error";
    return {Status::kError};
  }
  NEARBY_LOGS(INFO) << "Waiting for future to complete: " << method_name;
  ExceptionOr<Status> result = future->Get();
  if (!result.ok()) {
    NEARBY_LOGS(INFO) << "Future:[" << method_name
                      << "] completed with exception:" << result.exception();
    return {Status::kError};
  }
  NEARBY_LOGS(INFO) << "Future:[" << method_name
                    << "] completed with status:" << result.result().value;
  return result.result();
}

void BasePcpHandler::RunOnPcpHandlerThread(const std::string& name,
                                           Runnable runnable) {
  serial_executor_.Execute(name, std::move(runnable));
}

EncryptionRunner::ResultListener BasePcpHandler::GetResultListener() {
  return {
      .on_success_cb =
          [this](const std::string& endpoint_id,
                 std::unique_ptr<UKey2Handshake> ukey2,
                 const std::string& auth_token,
                 const ByteArray& raw_auth_token) {
            RunOnPcpHandlerThread(
                "encryption-success",
                [this, endpoint_id, raw_ukey2 = ukey2.release(), auth_token,
                 raw_auth_token]() RUN_ON_PCP_HANDLER_THREAD() mutable {
                  OnEncryptionSuccessRunnable(
                      endpoint_id, std::unique_ptr<UKey2Handshake>(raw_ukey2),
                      auth_token, raw_auth_token);
                });
          },
      .on_failure_cb =
          [this](const std::string& endpoint_id, EndpointChannel* channel) {
            RunOnPcpHandlerThread(
                "encryption-failure",
                [this, endpoint_id, channel]() RUN_ON_PCP_HANDLER_THREAD() {
                  NEARBY_LOGS(ERROR)
                      << "Encryption failed for endpoint_id=" << endpoint_id
                      << " on medium="
                      << location::nearby::proto::connections::Medium_Name(
                             channel->GetMedium());
                  OnEncryptionFailureRunnable(endpoint_id, channel);
                });
          },
  };
}

void BasePcpHandler::OnEncryptionSuccessRunnable(
    const std::string& endpoint_id, std::unique_ptr<UKey2Handshake> ukey2,
    const std::string& auth_token, const ByteArray& raw_auth_token) {
  // Quick fail if we've been removed from pending connections while we were
  // busy running UKEY2.
  auto it = pending_connections_.find(endpoint_id);
  if (it == pending_connections_.end()) {
    NEARBY_LOGS(INFO)
        << "Connection not found on UKEY negotination complete; endpoint_id="
        << endpoint_id;
    return;
  }

  BasePcpHandler::PendingConnectionInfo& connection_info = it->second;
  Medium medium = connection_info.channel->GetMedium();

  if (!ukey2) {
    // Fail early, if there is no crypto context.
    ProcessPreConnectionInitiationFailure(
        connection_info.client, medium, endpoint_id,
        connection_info.channel.get(), connection_info.is_incoming,
        connection_info.start_time, {Status::kEndpointIoError},
        connection_info.result.lock().get());
    return;
  }

  connection_info.SetCryptoContext(std::move(ukey2));
  connection_info.connection_token = GetHashedConnectionToken(raw_auth_token);
  NEARBY_LOGS(INFO)
      << "Register encrypted connection; wait for response; endpoint_id="
      << endpoint_id;

  // Set ourselves up so that we receive all acceptance/rejection messages
  endpoint_manager_->RegisterFrameProcessor(V1Frame::CONNECTION_RESPONSE, this);

  ConnectionOptions connection_options = connection_info.connection_options;
  connection_options.allowed =
      ComputeIntersectionOfSupportedMediums(connection_info);

  // Now we register our endpoint so that we can listen for both sides to
  // accept.
  LogConnectionAttemptSuccess(endpoint_id, connection_info);
  endpoint_manager_->RegisterEndpoint(
      connection_info.client, endpoint_id,
      {
          .remote_endpoint_info = connection_info.remote_endpoint_info,
          .authentication_token = auth_token,
          .raw_authentication_token = raw_auth_token,
          .is_incoming_connection = connection_info.is_incoming,
      },
      connection_options, std::move(connection_info.channel),
      connection_info.listener, connection_info.connection_token);

  if (auto future_status = connection_info.result.lock()) {
    NEARBY_LOGS(INFO) << "Connection established; Finalising future OK.";
    future_status->Set({Status::kSuccess});
    connection_info.result.reset();
  }
}

void BasePcpHandler::OnEncryptionFailureRunnable(
    const std::string& endpoint_id, EndpointChannel* endpoint_channel) {
  auto it = pending_connections_.find(endpoint_id);
  if (it == pending_connections_.end()) {
    NEARBY_LOGS(INFO)
        << "Connection not found on UKEY negotination complete; endpoint_id="
        << endpoint_id;
    return;
  }

  BasePcpHandler::PendingConnectionInfo& info = it->second;
  // We had a bug here, caused by a race with EncryptionRunner. We now verify
  // the EndpointChannel to avoid it. In a simultaneous connection, we clean
  // up one of the two EndpointChannels and then update our pendingConnections
  // with the winning channel's state. Closing a channel that was in the
  // middle of EncryptionRunner would trigger onEncryptionFailed, and, since
  // the map had already updated with the winning EndpointChannel, we closed
  // it too by accident.
  if (*endpoint_channel != *info.channel) {
    NEARBY_LOGS(INFO) << "Not destroying channel [mismatch]: passed="
                      << endpoint_channel->GetName()
                      << "; expected=" << info.channel->GetName();
    return;
  }

  ProcessPreConnectionInitiationFailure(
      info.client, info.channel->GetMedium(), endpoint_id, info.channel.get(),
      info.is_incoming, info.start_time, {Status::kEndpointIoError},
      info.result.lock().get());
}

ConnectionInfo BasePcpHandler::FillConnectionInfo(
    ClientProxy* client, const ConnectionRequestInfo& info,
    const ConnectionOptions& connection_options) {
  ConnectionInfo connection_info;
  connection_info.local_endpoint_id = client->GetLocalEndpointId();
  connection_info.local_endpoint_info = info.endpoint_info;
  connection_info.nonce = Prng().NextInt32();
  if (mediums_->GetWifi().IsAvailable()) {
    connection_info.supports_5_ghz =
        mediums_->GetWifi().GetCapability().supports_5_ghz;

    api::WifiInformation& wifi_info = mediums_->GetWifi().GetInformation();
    connection_info.bssid = wifi_info.bssid;
    connection_info.ap_frequency = wifi_info.ap_frequency;
    connection_info.ip_address = wifi_info.ip_address_4_bytes;
    NEARBY_LOGS(INFO) << "Query for WIFI information: is_supports_5_ghz="
                      << connection_info.supports_5_ghz
                      << "; bssid=" << connection_info.bssid
                      << "; ap_frequency=" << connection_info.ap_frequency
                      << "Mhz; ip_address in bytes format="
                      << connection_info.ip_address;
  }
  connection_info.supported_mediums =
      GetSupportedConnectionMediumsByPriority(connection_options);
  connection_info.keep_alive_interval_millis =
      connection_options.keep_alive_interval_millis;
  connection_info.keep_alive_timeout_millis =
      connection_options.keep_alive_timeout_millis;
  return connection_info;
}

Status BasePcpHandler::RequestConnection(
    ClientProxy* client, const std::string& endpoint_id,
    const ConnectionRequestInfo& info,
    const ConnectionOptions& connection_options) {
  auto result = std::make_shared<Future<Status>>();
  RunOnPcpHandlerThread(
      "request-connection",
      [this, client, &info, connection_options, endpoint_id,
       result]() RUN_ON_PCP_HANDLER_THREAD() {
        absl::Time start_time = SystemClock::ElapsedRealtime();

        // If we already have a pending connection, then we shouldn't allow any
        // more outgoing connections to this endpoint.
        if (pending_connections_.count(endpoint_id)) {
          NEARBY_LOGS(INFO)
              << "In requestConnection(), connection requested with "
                 "endpoint(id="
              << endpoint_id
              << "), but we already have a pending connection with them.";
          result->Set({Status::kAlreadyConnectedToEndpoint});
          return;
        }

        // If our child class says we can't send any more outgoing connections,
        // listen to them.
        if (ShouldEnforceTopologyConstraints(client->GetAdvertisingOptions()) &&
            !CanSendOutgoingConnection(client)) {
          NEARBY_LOGS(INFO)
              << "In requestConnection(), client=" << client->GetClientId()
              << " attempted a connection with endpoint(id=" << endpoint_id
              << "), but outgoing connections are disallowed";
          result->Set({Status::kOutOfOrderApiCall});
          return;
        }

        DiscoveredEndpoint* endpoint = GetDiscoveredEndpoint(endpoint_id);
        if (endpoint == nullptr) {
          NEARBY_LOGS(INFO)
              << "Discovered endpoint not found: endpoint_id=" << endpoint_id;
          result->Set({Status::kEndpointUnknown});
          return;
        }

        auto remote_bluetooth_mac_address = BluetoothUtils::ToString(
            connection_options.remote_bluetooth_mac_address);
        if (!remote_bluetooth_mac_address.empty()) {
          if (AppendRemoteBluetoothMacAddressEndpoint(
                  endpoint_id, remote_bluetooth_mac_address,
                  client->GetDiscoveryOptions()))
            NEARBY_LOGS(INFO)
                << "Appended remote Bluetooth MAC Address endpoint ["
                << remote_bluetooth_mac_address << "]";
        }

        if (AppendWebRTCEndpoint(endpoint_id, client->GetDiscoveryOptions()))
          NEARBY_LOGS(INFO) << "Appended Web RTC endpoint.";

        auto discovered_endpoints = GetDiscoveredEndpoints(endpoint_id);
        std::unique_ptr<EndpointChannel> channel;
        ConnectImplResult connect_impl_result;

        for (auto connect_endpoint : discovered_endpoints) {
          if (!MediumSupportedByClientOptions(connect_endpoint->medium,
                                              connection_options))
            continue;
          connect_impl_result = ConnectImpl(client, connect_endpoint);
          if (connect_impl_result.status.Ok()) {
            channel = std::move(connect_impl_result.endpoint_channel);
            break;
          }
        }

        Medium channel_medium =
            channel ? channel->GetMedium() : Medium::UNKNOWN_MEDIUM;
        if (channel == nullptr) {
          NEARBY_LOGS(INFO)
              << "Endpoint channel not available: endpoint_id=" << endpoint_id;
          ProcessPreConnectionInitiationFailure(
              client, channel_medium, endpoint_id, channel.get(),
              /* is_incoming = */ false, start_time, connect_impl_result.status,
              result.get());
          return;
        }

        NEARBY_LOGS(INFO)
            << "In requestConnection(), wrote ConnectionRequestFrame "
               "to endpoint_id="
            << endpoint_id;

        ConnectionInfo connection_info =
            FillConnectionInfo(client, info, connection_options);

        Exception write_exception =
            WriteConnectionRequestFrame(connection_info, channel.get());

        if (!write_exception.Ok()) {
          NEARBY_LOGS(INFO) << "Failed to send connection request: endpoint_id="
                            << endpoint_id;
          ProcessPreConnectionInitiationFailure(
              client, channel_medium, endpoint_id, channel.get(),
              /* is_incoming = */ false, start_time, {Status::kEndpointIoError},
              result.get());
          return;
        }

        NEARBY_LOGS(INFO) << "Adding connection to pending set: endpoint_id="
                          << endpoint_id;

        // We've successfully connected to the device, and are now about to jump
        // on to the EncryptionRunner thread to start running our encryption
        // protocol. We'll mark ourselves as pending in case we get another call
        // to RequestConnection or OnIncomingConnection, so that we can cancel
        // the connection if needed.
        // Not using designated initializers here since the VS C++ compiler
        // errors out indicating that MediumSelector<bool> is not an aggregate
        PendingConnectionInfo pendingConnectionInfo{};
        pendingConnectionInfo.client = client;
        pendingConnectionInfo.remote_endpoint_info = endpoint->endpoint_info;
        pendingConnectionInfo.nonce = connection_info.nonce;
        pendingConnectionInfo.is_incoming = false;
        pendingConnectionInfo.start_time = start_time;
        pendingConnectionInfo.listener = info.listener;
        pendingConnectionInfo.connection_options = connection_options;
        pendingConnectionInfo.result = result;
        pendingConnectionInfo.channel = std::move(channel);

        EndpointChannel* endpoint_channel =
            pending_connections_
                .emplace(endpoint_id, std::move(pendingConnectionInfo))
                .first->second.channel.get();

        NEARBY_LOGS(INFO) << "Initiating secure connection: endpoint_id="
                          << endpoint_id;
        // Next, we'll set up encryption. When it's done, our future will return
        // and RequestConnection() will finish.
        encryption_runner_.StartClient(client, endpoint_id, endpoint_channel,
                                       GetResultListener());
      });
  NEARBY_LOGS(INFO) << "Waiting for connection to complete: endpoint_id="
                    << endpoint_id;
  auto status =
      WaitForResult(absl::StrCat("RequestConnection(", endpoint_id, ")"),
                    client->GetClientId(), result.get());
  NEARBY_LOGS(INFO) << "Wait is complete: endpoint_id=" << endpoint_id
                    << "; status=" << status.value;
  return status;
}

bool BasePcpHandler::MediumSupportedByClientOptions(
    const location::nearby::proto::connections::Medium& medium,
    const ConnectionOptions& connection_options) const {
  for (auto supported_medium : connection_options.GetMediums()) {
    if (medium == supported_medium) {
      return true;
    }
  }
  return false;
}

// Get ordered supported connection medium based on local advertising/discovery
// option.
std::vector<location::nearby::proto::connections::Medium>
BasePcpHandler::GetSupportedConnectionMediumsByPriority(
    const ConnectionOptions& local_connection_option) {
  std::vector<location::nearby::proto::connections::Medium>
      supported_mediums_by_priority;
  for (auto medium_by_priority : GetConnectionMediumsByPriority()) {
    if (MediumSupportedByClientOptions(medium_by_priority,
                                       local_connection_option)) {
      supported_mediums_by_priority.push_back(medium_by_priority);
    }
  }
  return supported_mediums_by_priority;
}

void BasePcpHandler::StripOutUnavailableMediums(
    AdvertisingOptions& advertising_options) {
  BooleanMediumSelector& allowed = advertising_options.allowed;

  if (allowed.bluetooth) {
    allowed.bluetooth = mediums_->GetBluetoothClassic().IsAvailable();
  }
  if (allowed.ble) {
    allowed.ble = mediums_->GetBle().IsAvailable();
  }
  if (allowed.web_rtc) {
    allowed.web_rtc = mediums_->GetWebRtc().IsAvailable();
  }
  if (allowed.wifi_lan) {
    allowed.wifi_lan = mediums_->GetWifiLan().IsAvailable();
  }
  if (allowed.wifi_hotspot) {
    allowed.wifi_hotspot = mediums_->GetWifiHotspot().IsAPAvailable();
  }
  if (allowed.wifi_direct) {
    allowed.wifi_direct = mediums_->GetWifiDirect().IsGOAvailable();
  }
}

void BasePcpHandler::StripOutUnavailableMediums(
    DiscoveryOptions& discovery_options) {
  BooleanMediumSelector& allowed = discovery_options.allowed;

  if (allowed.bluetooth) {
    allowed.bluetooth = mediums_->GetBluetoothClassic().IsAvailable();
  }
  if (allowed.ble) {
    allowed.ble = mediums_->GetBle().IsAvailable();
  }
  if (allowed.web_rtc) {
    allowed.web_rtc = mediums_->GetWebRtc().IsAvailable();
  }
  if (allowed.wifi_lan) {
    allowed.wifi_lan = mediums_->GetWifiLan().IsAvailable();
  }
  if (allowed.wifi_hotspot) {
    allowed.wifi_hotspot = mediums_->GetWifi().IsAvailable() &&
                           mediums_->GetWifiHotspot().IsClientAvailable();
  }
  if (allowed.wifi_direct) {
    allowed.wifi_direct = mediums_->GetWifi().IsAvailable() &&
                          mediums_->GetWifiDirect().IsGCAvailable();
  }
}

// Get any single discovered endpoint for a given endpoint_id.
BasePcpHandler::DiscoveredEndpoint* BasePcpHandler::GetDiscoveredEndpoint(
    const std::string& endpoint_id) {
  auto it = discovered_endpoints_.find(endpoint_id);
  if (it == discovered_endpoints_.end()) {
    return nullptr;
  }
  return it->second.get();
}

std::vector<BasePcpHandler::DiscoveredEndpoint*>
BasePcpHandler::GetDiscoveredEndpoints(const std::string& endpoint_id) {
  std::vector<BasePcpHandler::DiscoveredEndpoint*> result;
  auto it = discovered_endpoints_.equal_range(endpoint_id);
  for (auto item = it.first; item != it.second; item++) {
    result.push_back(item->second.get());
  }
  std::sort(result.begin(), result.end(),
            [this](DiscoveredEndpoint* a, DiscoveredEndpoint* b) -> bool {
              return IsPreferred(*a, *b);
            });

  return result;
}

std::vector<BasePcpHandler::DiscoveredEndpoint*>
BasePcpHandler::GetDiscoveredEndpoints(
    const location::nearby::proto::connections::Medium medium) {
  std::vector<BasePcpHandler::DiscoveredEndpoint*> result;
  for (const auto& item : discovered_endpoints_) {
    if (item.second->medium == medium) {
      result.push_back(item.second.get());
    }
  }
  return result;
}

mediums::WebrtcPeerId BasePcpHandler::CreatePeerIdFromAdvertisement(
    const std::string& service_id, const std::string& endpoint_id,
    const ByteArray& endpoint_info) {
  std::string seed =
      absl::StrCat(service_id, endpoint_id, std::string(endpoint_info));
  return mediums::WebrtcPeerId::FromSeed(ByteArray(std::move(seed)));
}

bool BasePcpHandler::HasOutgoingConnections(ClientProxy* client) const {
  for (const auto& item : pending_connections_) {
    auto& connection = item.second;
    if (!connection.is_incoming) {
      return true;
    }
  }
  return client->GetNumOutgoingConnections() > 0;
}

bool BasePcpHandler::HasIncomingConnections(ClientProxy* client) const {
  for (const auto& item : pending_connections_) {
    auto& connection = item.second;
    if (connection.is_incoming) {
      return true;
    }
  }
  return client->GetNumIncomingConnections() > 0;
}

bool BasePcpHandler::CanSendOutgoingConnection(ClientProxy* client) const {
  return true;
}

bool BasePcpHandler::CanReceiveIncomingConnection(ClientProxy* client) const {
  return true;
}

Exception BasePcpHandler::WriteConnectionRequestFrame(
    const ConnectionInfo& conection_info, EndpointChannel* endpoint_channel) {
  return endpoint_channel->Write(parser::ForConnectionRequest(conection_info));
}

void BasePcpHandler::ProcessPreConnectionInitiationFailure(
    ClientProxy* client, Medium medium, const std::string& endpoint_id,
    EndpointChannel* channel, bool is_incoming, absl::Time start_time,
    Status status, Future<Status>* result) {
  if (channel != nullptr) {
    channel->Close();
  }

  if (result != nullptr) {
    NEARBY_LOGS(INFO) << "Connection failed; aborting future";
    result->Set(status);
  }

  LogConnectionAttemptFailure(client, medium, endpoint_id, is_incoming,
                              start_time, channel);
  // result is hold inside a swapper, and saved in PendingConnectionInfo.
  // PendingConnectionInfo destructor will clear the memory of SettableFuture
  // shared_ptr for result.
  pending_connections_.erase(endpoint_id);
}

void BasePcpHandler::ProcessPreConnectionResultFailure(
    ClientProxy* client, const std::string& endpoint_id) {
  auto item = pending_connections_.extract(endpoint_id);
  endpoint_manager_->DiscardEndpoint(client, endpoint_id);
  client->OnConnectionRejected(endpoint_id, {Status::kError});
}

bool BasePcpHandler::ShouldEnforceTopologyConstraints(
    const AdvertisingOptions& local_advertising_options) const {
  // Topology constraints only matter for the advertiser.
  // For discoverers, we'll always enforce them.
  if (local_advertising_options.strategy.IsNone()) {
    return true;
  }

  return local_advertising_options.enforce_topology_constraints;
}

bool BasePcpHandler::AutoUpgradeBandwidth(
    const AdvertisingOptions& local_advertising_options) const {
  if (local_advertising_options.strategy.IsNone()) {
    return true;
  }

  return local_advertising_options.auto_upgrade_bandwidth;
}

Status BasePcpHandler::AcceptConnection(
    ClientProxy* client, const std::string& endpoint_id,
    const PayloadListener& payload_listener) {
  Future<Status> response;
  RunOnPcpHandlerThread(
      "accept-connection", [this, client, endpoint_id, payload_listener,
                            &response]() RUN_ON_PCP_HANDLER_THREAD() {
        NEARBY_LOGS(INFO) << "AcceptConnection: endpoint_id=" << endpoint_id;
        if (!pending_connections_.count(endpoint_id)) {
          NEARBY_LOGS(INFO)
              << "AcceptConnection: no pending connection for endpoint_id="
              << endpoint_id;

          response.Set({Status::kEndpointUnknown});
          return;
        }
        auto& connection_info = pending_connections_[endpoint_id];

        // By this point in the flow, connection_info.channel has been
        // nulled out because ownership of that EndpointChannel was passed on to
        // EndpointChannelManager via a call to
        // EndpointManager::registerEndpoint(), so we now need to get access to
        // the EndpointChannel from the authoritative owner.
        std::shared_ptr<EndpointChannel> channel =
            channel_manager_->GetChannelForEndpoint(endpoint_id);
        if (channel == nullptr) {
          NEARBY_LOGS(ERROR) << "Channel destroyed before Accept; bring down "
                                "connection: endpoint_id="
                             << endpoint_id;
          ProcessPreConnectionResultFailure(client, endpoint_id);
          response.Set({Status::kEndpointUnknown});
          return;
        }

        Exception write_exception =
            channel->Write(parser::ForConnectionResponse(
                Status::kSuccess, client->GetLocalOsInfo()));
        if (!write_exception.Ok()) {
          NEARBY_LOGS(INFO)
              << "AcceptConnection: failed to send response: endpoint_id="
              << endpoint_id;
          ProcessPreConnectionResultFailure(client, endpoint_id);
          response.Set({Status::kEndpointIoError});
          return;
        }

        NEARBY_LOGS(INFO) << "AcceptConnection: accepting locally: endpoint_id="
                          << endpoint_id;
        connection_info.LocalEndpointAcceptedConnection(endpoint_id,
                                                        payload_listener);
        EvaluateConnectionResult(client, endpoint_id,
                                 false /* can_close_immediately */);
        response.Set({Status::kSuccess});
      });

  return WaitForResult(absl::StrCat("AcceptConnection(", endpoint_id, ")"),
                       client->GetClientId(), &response);
}

Status BasePcpHandler::RejectConnection(ClientProxy* client,
                                        const std::string& endpoint_id) {
  Future<Status> response;
  RunOnPcpHandlerThread(
      "reject-connection",
      [this, client, endpoint_id, &response]() RUN_ON_PCP_HANDLER_THREAD() {
        NEARBY_LOG(INFO, "RejectConnection: id=%s", endpoint_id.c_str());
        if (!pending_connections_.count(endpoint_id)) {
          NEARBY_LOGS(INFO)
              << "RejectConnection: no pending connection for endpoint_id="
              << endpoint_id;
          response.Set({Status::kEndpointUnknown});
          return;
        }
        auto& connection_info = pending_connections_[endpoint_id];

        // By this point in the flow, connection_info->endpoint_channel_ has
        // been nulled out because ownership of that EndpointChannel was passed
        // on to EndpointChannelManager via a call to
        // EndpointManager::registerEndpoint(), so we now need to get access to
        // the EndpointChannel from the authoritative owner.
        std::shared_ptr<EndpointChannel> channel =
            channel_manager_->GetChannelForEndpoint(endpoint_id);
        if (channel == nullptr) {
          NEARBY_LOGS(ERROR)
              << "Channel destroyed before Reject; bring down connection: "
                 "endpoint_id="
              << endpoint_id;
          ProcessPreConnectionResultFailure(client, endpoint_id);
          response.Set({Status::kEndpointUnknown});
          return;
        }

        Exception write_exception =
            channel->Write(parser::ForConnectionResponse(
                Status::kConnectionRejected, client->GetLocalOsInfo()));
        if (!write_exception.Ok()) {
          NEARBY_LOGS(INFO)
              << "RejectConnection: failed to send response: endpoint_id="
              << endpoint_id;
          ProcessPreConnectionResultFailure(client, endpoint_id);
          response.Set({Status::kEndpointIoError});
          return;
        }

        NEARBY_LOGS(INFO) << "RejectConnection: rejecting locally: endpoint_id="
                          << endpoint_id;
        connection_info.LocalEndpointRejectedConnection(endpoint_id);
        EvaluateConnectionResult(client, endpoint_id,
                                 false /* can_close_immediately */);
        response.Set({Status::kSuccess});
      });

  return WaitForResult(absl::StrCat("RejectConnection(", endpoint_id, ")"),
                       client->GetClientId(), &response);
}

void BasePcpHandler::OnIncomingFrame(
    OfflineFrame& frame, const std::string& endpoint_id, ClientProxy* client,
    location::nearby::proto::connections::Medium medium,
    PacketMetaData& packet_meta_data) {
  CountDownLatch latch(1);
  RunOnPcpHandlerThread(
      "incoming-frame",
      [this, client, endpoint_id, frame, &latch]() RUN_ON_PCP_HANDLER_THREAD() {
        NEARBY_LOGS(INFO) << "OnConnectionResponse: endpoint_id="
                          << endpoint_id;

        if (client->HasRemoteEndpointResponded(endpoint_id)) {
          NEARBY_LOGS(INFO)
              << "OnConnectionResponse: already handled; endpoint_id="
              << endpoint_id;
          return;
        }

        const ConnectionResponseFrame& connection_response =
            frame.v1().connection_response();

        // For backward compatible, here still check both status and
        // response parameters until the response feature is roll out in all
        // supported devices.
        bool accepted = false;
        if (connection_response.has_response()) {
          accepted =
              connection_response.response() == ConnectionResponseFrame::ACCEPT;
        } else {
          accepted = connection_response.status() == Status::kSuccess;
        }
        if (accepted) {
          NEARBY_LOGS(INFO)
              << "OnConnectionResponse: remote accepted; endpoint_id="
              << endpoint_id;
          client->RemoteEndpointAcceptedConnection(endpoint_id);
        } else {
          NEARBY_LOGS(INFO)
              << "OnConnectionResponse: remote rejected; endpoint_id="
              << endpoint_id << "; status=" << connection_response.status();
          client->RemoteEndpointRejectedConnection(endpoint_id);
        }

        if (connection_response.has_os_info()) {
          client->SetRemoteOsInfo(endpoint_id, connection_response.os_info());
        }

        EvaluateConnectionResult(client, endpoint_id,
                                 /* can_close_immediately= */ true);

        latch.CountDown();
      });
  WaitForLatch("OnIncomingFrame()", &latch);
}

void BasePcpHandler::OnEndpointDisconnect(ClientProxy* client,
                                          const std::string& service_id,
                                          const std::string& endpoint_id,
                                          CountDownLatch barrier) {
  if (stop_.Get()) {
    barrier.CountDown();
    return;
  }
  RunOnPcpHandlerThread("on-endpoint-disconnect",
                        [this, client, endpoint_id, barrier]()
                            RUN_ON_PCP_HANDLER_THREAD() mutable {
                              auto item = pending_alarms_.find(endpoint_id);
                              if (item != pending_alarms_.end()) {
                                auto& alarm = item->second;
                                alarm->Cancel();
                                pending_alarms_.erase(item);
                              }
                              ProcessPreConnectionResultFailure(client,
                                                                endpoint_id);
                              barrier.CountDown();
                            });
}

BluetoothDevice BasePcpHandler::GetRemoteBluetoothDevice(
    const std::string& remote_bluetooth_mac_address) {
  return mediums_->GetBluetoothClassic().GetRemoteDevice(
      remote_bluetooth_mac_address);
}

void BasePcpHandler::OnEndpointFound(
    ClientProxy* client, std::shared_ptr<DiscoveredEndpoint> endpoint) {
  // Check if we've seen this endpoint ID before.
  std::string& endpoint_id = endpoint->endpoint_id;
  NEARBY_LOGS(INFO) << "OnEndpointFound: id=" << endpoint_id << " [enter]";

  auto range = discovered_endpoints_.equal_range(endpoint->endpoint_id);
  bool is_range_empty = range.first == range.second;
  DiscoveredEndpoint* owned_endpoint = nullptr;
  for (auto& item = range.first; item != range.second; ++item) {
    auto& discovered_endpoint = item->second;
    if (discovered_endpoint->medium != endpoint->medium) continue;
    // Check if there was a info change. If there was, report the previous
    // endpoint as lost.
    if (discovered_endpoint->endpoint_info != endpoint->endpoint_info) {
      OnEndpointLost(client, *discovered_endpoint);
      discovered_endpoint = endpoint;  // Replace endpoint.
      OnEndpointFound(client, std::move(endpoint));
      return;
    } else {
      owned_endpoint = endpoint.get();
      break;
    }
  }

  if (!owned_endpoint) {
    owned_endpoint =
        discovered_endpoints_.emplace(endpoint_id, std::move(endpoint))
            ->second.get();
  }

  // Range is empty: this is the first endpoint we discovered so far.
  // Report this endpoint_id to client.
  if (is_range_empty) {
    NEARBY_LOGS(INFO) << "Adding new endpoint: endpoint_id=" << endpoint_id;
    // And, as it's the first time, report it to the client.
    client->OnEndpointFound(
        owned_endpoint->service_id, owned_endpoint->endpoint_id,
        owned_endpoint->endpoint_info, owned_endpoint->medium);
  } else {
    NEARBY_LOGS(INFO) << "Adding new medium for endpoint: endpoint_id="
                      << endpoint_id << "; medium=" << owned_endpoint->medium;
  }
}

void BasePcpHandler::OnEndpointLost(
    ClientProxy* client, const BasePcpHandler::DiscoveredEndpoint& endpoint) {
  // Look up the DiscoveredEndpoint we have in our cache.
  const auto* discovered_endpoint = GetDiscoveredEndpoint(endpoint.endpoint_id);
  if (discovered_endpoint == nullptr) {
    NEARBY_LOGS(INFO) << "No previous endpoint (nothing to lose): endpoint_id="
                      << endpoint.endpoint_id;
    return;
  }

  // Validate that the cached endpoint has the same info as the one reported as
  // onLost. If the info differs, then no-op. This likely means that the remote
  // device changed their info. We reported onFound for the new info and are
  // just now figuring out that we lost the old info.
  if (discovered_endpoint->endpoint_info != endpoint.endpoint_info) {
    NEARBY_LOGS(INFO) << "Previous endpoint name mismatch; passed="
                      << absl::BytesToHexString(endpoint.endpoint_info.data())
                      << "; expected="
                      << absl::BytesToHexString(
                             discovered_endpoint->endpoint_info.data());
    return;
  }

  auto item = discovered_endpoints_.extract(endpoint.endpoint_id);
  if (!discovered_endpoints_.count(endpoint.endpoint_id)) {
    client->OnEndpointLost(endpoint.service_id, endpoint.endpoint_id);
  }
}

bool BasePcpHandler::IsPreferred(
    const BasePcpHandler::DiscoveredEndpoint& new_endpoint,
    const BasePcpHandler::DiscoveredEndpoint& old_endpoint) {
  std::vector<location::nearby::proto::connections::Medium> mediums =
      GetConnectionMediumsByPriority();
  // Make sure the comparator is irreflexive, so we have a strict weak ordering.
  if (new_endpoint.medium != old_endpoint.medium) {
    // As we iterate through the list of mediums, we see if we run into the new
    // endpoint's medium or the old endpoint's medium first.
    for (const auto& medium : mediums) {
      if (medium == new_endpoint.medium) {
        // The new endpoint's medium came first. It's preferred!
        return true;
      }

      if (medium == old_endpoint.medium) {
        // The old endpoint's medium came first. Stick with the old endpoint!
        return false;
      }
    }
  }
  std::string medium_string;
  for (const auto& medium : mediums) {
    absl::StrAppend(&medium_string, medium, "; ");
  }
  NEARBY_LOGS(ERROR) << "Failed to find either " << new_endpoint.medium
                     << " or " << old_endpoint.medium
                     << " in the list of locally supported mediums despite "
                        "expecting to find both, when deciding which medium "
                     << medium_string << " is preferred.";
  return false;
}

Exception BasePcpHandler::OnIncomingConnection(
    ClientProxy* client, const ByteArray& remote_endpoint_info,
    std::unique_ptr<EndpointChannel> channel,
    location::nearby::proto::connections::Medium medium) {
  absl::Time start_time = SystemClock::ElapsedRealtime();

  //  Fixes an NPE in ClientProxy.OnConnectionAccepted. The crash happened when
  //  the client stopped advertising and we nulled out state, followed by an
  //  incoming connection where we attempted to check that state.
  if (!client->IsAdvertising()) {
    NEARBY_LOGS(WARNING) << "Ignoring incoming connection on medium "
                         << location::nearby::proto::connections::Medium_Name(
                                channel->GetMedium())
                         << " because client=" << client->GetClientId()
                         << " is no longer advertising.";
    return {Exception::kIo};
  }

  // Endpoints connecting to us will always tell us about themselves first.
  ExceptionOr<OfflineFrame> wrapped_frame =
      ReadConnectionRequestFrame(channel.get());

  if (!wrapped_frame.ok()) {
    if (wrapped_frame.exception()) {
      NEARBY_LOGS(ERROR)
          << "Failed to parse incoming connection request; client="
          << client->GetClientId()
          << "; device=" << absl::BytesToHexString(remote_endpoint_info.data());
      ProcessPreConnectionInitiationFailure(
          client, medium, "", channel.get(),
          /* is_incoming= */ false, start_time, {Status::kError}, nullptr);
      return {Exception::kSuccess};
    }
    return wrapped_frame.GetException();
  }

  OfflineFrame& frame = wrapped_frame.result();
  const ConnectionRequestFrame& connection_request =
      frame.v1().connection_request();
  NEARBY_LOGS(INFO) << "In onIncomingConnection("
                    << location::nearby::proto::connections::Medium_Name(
                           channel->GetMedium())
                    << ") for client=" << client->GetClientId()
                    << ", read ConnectionRequestFrame from endpoint(id="
                    << connection_request.endpoint_id() << ")";
  if (client->IsConnectedToEndpoint(connection_request.endpoint_id())) {
    NEARBY_LOGS(ERROR) << "Incoming connection on medium "
                       << location::nearby::proto::connections::Medium_Name(
                              channel->GetMedium())
                       << " was denied because we're "
                          "already connected to endpoint(id="
                       << connection_request.endpoint_id() << ").";
    return {Exception::kIo};
  }

  // If we've already sent out a connection request to this endpoint, then this
  // is where we need to decide which connection to break.
  if (BreakTie(client, connection_request.endpoint_id(),
               connection_request.nonce(), channel.get())) {
    return {Exception::kSuccess};
  }

  // If our child class says we can't accept any more incoming connections,
  // listen to them.
  if (ShouldEnforceTopologyConstraints(client->GetAdvertisingOptions()) &&
      !CanReceiveIncomingConnection(client)) {
    NEARBY_LOGS(ERROR) << "Incoming connections are currently disallowed.";
    return {Exception::kIo};
  }

  // The ConnectionRequest frame has two fields that both contain the
  // EndpointInfo. The legacy field stores it as a string while the newer field
  // stores it as a byte array. We'll attempt to grab from the newer field, but
  // will accept the older string if it's all that exists.
  const ByteArray endpoint_info{connection_request.has_endpoint_info()
                                    ? connection_request.endpoint_info()
                                    : connection_request.endpoint_name()};

  // Retrieve the keep-alive frame interval and timeout fields. If the frame
  // doesn't have those fields, we need to get them as default from feature
  // flags to prevent 0-values causing thread ill.
  ConnectionOptions connection_options;
  connection_options.keep_alive_interval_millis = 0;
  connection_options.keep_alive_timeout_millis = 0;
  if (connection_request.has_keep_alive_interval_millis() &&
      connection_request.has_keep_alive_timeout_millis()) {
    connection_options.keep_alive_interval_millis =
        connection_request.keep_alive_interval_millis();
    connection_options.keep_alive_timeout_millis =
        connection_request.keep_alive_timeout_millis();
  }
  if (connection_options.keep_alive_interval_millis == 0 ||
      connection_options.keep_alive_timeout_millis == 0 ||
      connection_options.keep_alive_interval_millis >=
          connection_options.keep_alive_timeout_millis) {
    NEARBY_LOGS(WARNING)
        << "Incoming connection has wrong keep-alive frame interval="
        << connection_options.keep_alive_interval_millis
        << ", timeout=" << connection_options.keep_alive_timeout_millis
        << " values; correct them as default.",
        connection_options.keep_alive_interval_millis =
            FeatureFlags::GetInstance().GetFlags().keep_alive_interval_millis;
    connection_options.keep_alive_timeout_millis =
        FeatureFlags::GetInstance().GetFlags().keep_alive_timeout_millis;
  }

  const MediumMetadata& medium_metadata = connection_request.medium_metadata();
  ConnectionInfo& connection_info = connection_options.connection_info;
  connection_info.supports_5_ghz = medium_metadata.supports_5_ghz();
  connection_info.bssid = medium_metadata.bssid();
  connection_info.ap_frequency = medium_metadata.ap_frequency();
  connection_info.ip_address = medium_metadata.ip_address();
  NEARBY_LOGS(INFO) << connection_request.endpoint_id()
                    << "'s WIFI information: is_supports_5_ghz="
                    << connection_info.supports_5_ghz
                    << "; bssid=" << connection_info.bssid
                    << "; ap_frequency=" << connection_info.ap_frequency
                    << "Mhz; ip_address in bytes format="
                    << connection_info.ip_address;

  // We've successfully connected to the device, and are now about to jump on to
  // the EncryptionRunner thread to start running our encryption protocol. We'll
  // mark ourselves as pending in case we get another call to RequestConnection
  // or OnIncomingConnection, so that we can cancel the connection if needed.
  // Not using designated initializers here since the VS C++ compiler errors
  // out indicating that MediumSelector<bool> is not an aggregate
  PendingConnectionInfo pendingConnectionInfo{};
  pendingConnectionInfo.client = client;
  pendingConnectionInfo.remote_endpoint_info = endpoint_info;
  pendingConnectionInfo.nonce = connection_request.nonce();
  pendingConnectionInfo.is_incoming = true;
  pendingConnectionInfo.start_time = start_time;
  pendingConnectionInfo.listener = advertising_listener_;
  pendingConnectionInfo.connection_options = connection_options;
  pendingConnectionInfo.supported_mediums =
      parser::ConnectionRequestMediumsToMediums(connection_request);
  pendingConnectionInfo.channel = std::move(channel);

  auto* owned_channel = pending_connections_
                            .emplace(connection_request.endpoint_id(),
                                     std::move(pendingConnectionInfo))
                            .first->second.channel.get();

  // Next, we'll set up encryption.
  encryption_runner_.StartServer(client, connection_request.endpoint_id(),
                                 owned_channel, GetResultListener());
  return {Exception::kSuccess};
}

bool BasePcpHandler::BreakTie(ClientProxy* client,
                              const std::string& endpoint_id,
                              std::int32_t incoming_nonce,
                              EndpointChannel* endpoint_channel) {
  auto it = pending_connections_.find(endpoint_id);
  if (it != pending_connections_.end()) {
    BasePcpHandler::PendingConnectionInfo& info = it->second;

    NEARBY_LOGS(INFO)
        << "In onIncomingConnection("
        << location::nearby::proto::connections::Medium_Name(
               endpoint_channel->GetMedium())
        << ") for client=" << client->GetClientId()
        << ", found a collision with endpoint " << endpoint_id
        << ". We've already sent a connection request to them with nonce "
        << info.nonce
        << ", but they're also trying to connect to us with nonce "
        << incoming_nonce;
    // Break the lowest connection. In the (extremely) rare case of a tie, break
    // both.
    if (info.nonce > incoming_nonce) {
      // Our connection won! Clean up their connection.
      endpoint_channel->Close();

      NEARBY_LOGS(INFO) << "In onIncomingConnection("
                        << location::nearby::proto::connections::Medium_Name(
                               endpoint_channel->GetMedium())
                        << ") for client=" << client->GetClientId()
                        << ", cleaned up the collision with endpoint "
                        << endpoint_id << " by closing their channel.";
      return true;
    } else if (info.nonce < incoming_nonce) {
      // Aw, we lost. Clean up our connection, and then we'll let their
      // connection continue on.
      ProcessTieBreakLoss(client, endpoint_id, &info);
      NEARBY_LOGS(INFO)
          << "In onIncomingConnection("
          << location::nearby::proto::connections::Medium_Name(
                 endpoint_channel->GetMedium())
          << ") for client=" << client->GetClientId()
          << ", cleaned up the collision with endpoint " << endpoint_id
          << " by closing our channel and notifying our client of the failure.";
    } else {
      // Oh. Huh. We both lost. Well, that's awkward. We'll clean up both and
      // just force the devices to retry.
      endpoint_channel->Close();

      ProcessTieBreakLoss(client, endpoint_id, &info);

      NEARBY_LOGS(INFO)
          << "In onIncomingConnection("
          << location::nearby::proto::connections::Medium_Name(
                 endpoint_channel->GetMedium())
          << ") for client=" << client->GetClientId()
          << ", cleaned up the collision with endpoint " << endpoint_id
          << " by closing both channels. Our nonces were identical, so we "
             "couldn't decide which channel to use.";
      return true;
    }
  }

  return false;
}

void BasePcpHandler::ProcessTieBreakLoss(
    ClientProxy* client, const std::string& endpoint_id,
    BasePcpHandler::PendingConnectionInfo* info) {
  ProcessPreConnectionInitiationFailure(
      client, info->channel->GetMedium(), endpoint_id, info->channel.get(),
      info->is_incoming, info->start_time, {Status::kEndpointIoError},
      info->result.lock().get());
  ProcessPreConnectionResultFailure(client, endpoint_id);
}

bool BasePcpHandler::AppendRemoteBluetoothMacAddressEndpoint(
    const std::string& endpoint_id,
    const std::string& remote_bluetooth_mac_address,
    const DiscoveryOptions& local_discovery_options) {
  if (!local_discovery_options.allowed.bluetooth) {
    return false;
  }

  auto it = discovered_endpoints_.equal_range(endpoint_id);
  if (it.first == it.second) {
    return false;
  }
  auto endpoint = it.first->second.get();
  for (auto item = it.first; item != it.second; item++) {
    if (item->second->medium ==
        location::nearby::proto::connections::Medium::BLUETOOTH) {
      NEARBY_LOGS(INFO)
          << "Cannot append remote Bluetooth MAC Address endpoint, because "
             "the endpoint has already been found over Bluetooth ["
          << remote_bluetooth_mac_address << "]";
      return false;
    }
  }

  auto remote_bluetooth_device =
      GetRemoteBluetoothDevice(remote_bluetooth_mac_address);
  if (!remote_bluetooth_device.IsValid()) {
    NEARBY_LOGS(INFO)
        << "Cannot append remote Bluetooth MAC Address endpoint, because a "
           "valid Bluetooth device could not be derived ["
        << remote_bluetooth_mac_address << "]";
    return false;
  }

  auto bluetooth_endpoint =
      std::make_shared<BluetoothEndpoint>(BluetoothEndpoint{
          {endpoint_id, endpoint->endpoint_info, endpoint->service_id,
           location::nearby::proto::connections::Medium::BLUETOOTH,
           WebRtcState::kUnconnectable},
          remote_bluetooth_device,
      });

  discovered_endpoints_.emplace(endpoint_id, std::move(bluetooth_endpoint));
  return true;
}

bool BasePcpHandler::AppendWebRTCEndpoint(
    const std::string& endpoint_id,
    const DiscoveryOptions& local_discovery_options) {
  if (!local_discovery_options.allowed.web_rtc) {
    return false;
  }

  bool should_connect_web_rtc = false;
  auto it = discovered_endpoints_.equal_range(endpoint_id);
  if (it.first == it.second) return false;
  auto endpoint = it.first->second.get();
  for (auto item = it.first; item != it.second; item++) {
    if (item->second->web_rtc_state != WebRtcState::kUnconnectable) {
      should_connect_web_rtc = true;
      break;
    }
  }
  if (!should_connect_web_rtc) return false;

  auto webrtc_endpoint = std::make_shared<WebRtcEndpoint>(WebRtcEndpoint{
      {endpoint_id, endpoint->endpoint_info, endpoint->service_id,
       location::nearby::proto::connections::Medium::WEB_RTC,
       WebRtcState::kConnectable},
      CreatePeerIdFromAdvertisement(endpoint->service_id, endpoint->endpoint_id,
                                    endpoint->endpoint_info),
  });

  discovered_endpoints_.emplace(endpoint_id, std::move(webrtc_endpoint));
  return true;
}

void BasePcpHandler::EvaluateConnectionResult(ClientProxy* client,
                                              const std::string& endpoint_id,
                                              bool can_close_immediately) {
  // Short-circuit immediately if we're not in an actionable state yet. We will
  // be called again once the other side has made their decision.
  if (!client->IsConnectionAccepted(endpoint_id) &&
      !client->IsConnectionRejected(endpoint_id)) {
    if (!client->HasLocalEndpointResponded(endpoint_id)) {
      NEARBY_LOGS(INFO)
          << "ConnectionResult: local client did not respond; endpoint_id="
          << endpoint_id;
    } else if (!client->HasRemoteEndpointResponded(endpoint_id)) {
      NEARBY_LOGS(INFO)
          << "ConnectionResult: remote client did not respond; endpoint_id="
          << endpoint_id;
    }
    return;
  }

  // Clean up the endpoint channel from our list of 'pending' connections. It's
  // no longer pending.
  auto it = pending_connections_.find(endpoint_id);
  if (it == pending_connections_.end()) {
    NEARBY_LOGS(INFO) << "No pending connection to evaluate; endpoint_id="
                      << endpoint_id;
    return;
  }

  auto pair = pending_connections_.extract(it);
  BasePcpHandler::PendingConnectionInfo& connection_info = pair.mapped();
  bool is_connection_accepted = client->IsConnectionAccepted(endpoint_id);

  Status response_code;
  if (is_connection_accepted) {
    NEARBY_LOGS(INFO) << "Pending connection accepted; endpoint_id="
                      << endpoint_id;
    response_code = {Status::kSuccess};

    // Both sides have accepted, so we can now start talking over encrypted
    // channels
    // Now, after both parties accepted connection (presumably after verifying &
    // matching security tokens), we are allowed to extract the shared key.
    auto ukey2 = std::move(connection_info.ukey2);
    bool succeeded = ukey2->VerifyHandshake();
    CHECK(succeeded);  // If this fails, it's a UKEY2 protocol bug.
    auto context = ukey2->ToConnectionContext();
    CHECK(context);  // there is no way how this can fail, if Verify succeeded.
    // If it did, it's a UKEY2 protocol bug.

    if (!channel_manager_->EncryptChannelForEndpoint(endpoint_id,
                                                     std::move(context))) {
      response_code = {Status::kEndpointUnknown};
    }
  } else {
    NEARBY_LOGS(INFO) << "Pending connection rejected; endpoint_id="
                      << endpoint_id;
    response_code = {Status::kConnectionRejected};
  }

  // If the connection failed, clean everything up and short circuit.
  if (!response_code.Ok()) {
    client->OnConnectionRejected(endpoint_id, response_code);

    // Clean up the channel in EndpointManager if it's no longer required.
    if (can_close_immediately) {
      endpoint_manager_->DiscardEndpoint(client, endpoint_id);
    } else {
      pending_alarms_.emplace(
          endpoint_id,
          std::make_unique<CancelableAlarm>(
              "BasePcpHandler.evaluateConnectionResult() delayed close",
              [this, client, endpoint_id]() {
                endpoint_manager_->DiscardEndpoint(client, endpoint_id);
              },
              kRejectedConnectionCloseDelay, &alarm_executor_));
    }

    return;
  }

  Medium medium =
      channel_manager_->GetChannelForEndpoint(endpoint_id)->GetMedium();
  client->GetAnalyticsRecorder().OnConnectionEstablished(
      endpoint_id, medium, connection_info.connection_token);

  // Invoke the client callback to let it know of the connection result.
  client->OnConnectionAccepted(endpoint_id);

  // Report the current bandwidth to the client
  client->OnBandwidthChanged(endpoint_id, medium);

  NEARBY_LOGS(INFO) << "Connection accepted on Medium:"
                    << location::nearby::proto::connections::Medium_Name(
                           medium);

  // Kick off the bandwidth upgrade for incoming connections.
  if (connection_info.is_incoming &&
      AutoUpgradeBandwidth(client->GetAdvertisingOptions())) {
    bwu_manager_->InitiateBwuForEndpoint(client, endpoint_id);
  }
}

ExceptionOr<OfflineFrame> BasePcpHandler::ReadConnectionRequestFrame(
    EndpointChannel* endpoint_channel) {
  if (endpoint_channel == nullptr) {
    return ExceptionOr<OfflineFrame>(Exception::kIo);
  }

  // To avoid a device connecting but never sending their introductory frame, we
  // time out the connection after a certain amount of time.
  CancelableAlarm timeout_alarm(
      absl::StrCat("PcpHandler(", this->GetStrategy().GetName(),
                   ")::ReadConnectionRequestFrame"),
      [endpoint_channel]() { endpoint_channel->Close(); },
      kConnectionRequestReadTimeout, &alarm_executor_);
  // Do a blocking read to try and find the ConnectionRequestFrame
  ExceptionOr<ByteArray> wrapped_bytes = endpoint_channel->Read();
  timeout_alarm.Cancel();

  if (!wrapped_bytes.ok()) {
    return ExceptionOr<OfflineFrame>(wrapped_bytes.exception());
  }

  ByteArray bytes = std::move(wrapped_bytes.result());
  ExceptionOr<OfflineFrame> wrapped_frame = parser::FromBytes(bytes);
  if (wrapped_frame.GetException().Raised(Exception::kInvalidProtocolBuffer)) {
    return ExceptionOr<OfflineFrame>(Exception::kIo);
  }

  OfflineFrame& frame = wrapped_frame.result();
  if (V1Frame::CONNECTION_REQUEST != parser::GetFrameType(frame)) {
    return ExceptionOr<OfflineFrame>(Exception::kIo);
  }

  return wrapped_frame;
}

std::string BasePcpHandler::GetHashedConnectionToken(
    const ByteArray& token_bytes) {
  auto token = std::string(token_bytes);
  return nearby::Base64Utils::Encode(Utils::Sha256Hash(token, token.size()))
      .substr(0, kConnectionTokenLength);
}

void BasePcpHandler::LogConnectionAttemptFailure(
    ClientProxy* client, Medium medium, const std::string& endpoint_id,
    bool is_incoming, absl::Time start_time,
    EndpointChannel* endpoint_channel) {
  location::nearby::proto::connections::ConnectionAttemptResult result =
      Cancelled(client, endpoint_id)
          ? location::nearby::proto::connections::RESULT_CANCELLED
          : location::nearby::proto::connections::RESULT_ERROR;
  std::unique_ptr<ConnectionAttemptMetadataParams>
      connections_attempt_metadata_params;
  if (endpoint_channel != nullptr) {
    connections_attempt_metadata_params =
        client->GetAnalyticsRecorder().BuildConnectionAttemptMetadataParams(
            endpoint_channel->GetTechnology(), endpoint_channel->GetBand(),
            endpoint_channel->GetFrequency(), endpoint_channel->GetTryCount());
  }
  if (is_incoming) {
    client->GetAnalyticsRecorder().OnIncomingConnectionAttempt(
        location::nearby::proto::connections::INITIAL, medium, result,
        SystemClock::ElapsedRealtime() - start_time,
        /* connection_token= */ "", connections_attempt_metadata_params.get());
  } else {
    client->GetAnalyticsRecorder().OnOutgoingConnectionAttempt(
        endpoint_id, location::nearby::proto::connections::INITIAL, medium,
        result, SystemClock::ElapsedRealtime() - start_time,
        /* connection_token= */ "", connections_attempt_metadata_params.get());
  }
}

void BasePcpHandler::LogConnectionAttemptSuccess(
    const std::string& endpoint_id,
    const PendingConnectionInfo& connection_info) {
  std::unique_ptr<ConnectionAttemptMetadataParams>
      connections_attempt_metadata_params;
  if (connection_info.channel != nullptr) {
    connections_attempt_metadata_params =
        connection_info.client->GetAnalyticsRecorder()
            .BuildConnectionAttemptMetadataParams(
                connection_info.channel->GetTechnology(),
                connection_info.channel->GetBand(),
                connection_info.channel->GetFrequency(),
                connection_info.channel->GetTryCount());
  } else {
    NEARBY_LOG(ERROR,
               "PendingConnectionInfo channel is null for "
               "LogConnectionAttemptSuccess. Bail out.");
    return;
  }
  if (connection_info.is_incoming) {
    connection_info.client->GetAnalyticsRecorder().OnIncomingConnectionAttempt(
        location::nearby::proto::connections::INITIAL,
        connection_info.channel->GetMedium(),
        location::nearby::proto::connections::RESULT_SUCCESS,
        SystemClock::ElapsedRealtime() - connection_info.start_time,
        connection_info.connection_token,
        connections_attempt_metadata_params.get());
  } else {
    connection_info.client->GetAnalyticsRecorder().OnOutgoingConnectionAttempt(
        endpoint_id, location::nearby::proto::connections::INITIAL,
        connection_info.channel->GetMedium(),
        location::nearby::proto::connections::RESULT_SUCCESS,
        SystemClock::ElapsedRealtime() - connection_info.start_time,
        connection_info.connection_token,
        connections_attempt_metadata_params.get());
  }
}

bool BasePcpHandler::Cancelled(ClientProxy* client,
                               const std::string& endpoint_id) {
  if (endpoint_id.empty()) {
    return false;
  }

  return client->GetCancellationFlag(endpoint_id)->Cancelled();
}

///////////////////// BasePcpHandler::PendingConnectionInfo ///////////////////

void BasePcpHandler::PendingConnectionInfo::SetCryptoContext(
    std::unique_ptr<UKey2Handshake> ukey2) {
  this->ukey2 = std::move(ukey2);
}

BasePcpHandler::PendingConnectionInfo::~PendingConnectionInfo() {
  auto future_status = result.lock();
  if (future_status && !future_status->IsSet()) {
    NEARBY_LOG(INFO, "Future was not set; destroying info");
    future_status->Set({Status::kError});
  }

  if (channel != nullptr) {
    channel->Close(
        location::nearby::proto::connections::DisconnectionReason::SHUTDOWN);
  }

  // Destroy crypto context now; for some reason, crypto context destructor
  // segfaults if it is not destroyed here.
  this->ukey2.reset();
}

void BasePcpHandler::PendingConnectionInfo::LocalEndpointAcceptedConnection(
    const std::string& endpoint_id, const PayloadListener& payload_listener) {
  client->LocalEndpointAcceptedConnection(endpoint_id, payload_listener);
}

void BasePcpHandler::PendingConnectionInfo::LocalEndpointRejectedConnection(
    const std::string& endpoint_id) {
  client->LocalEndpointRejectedConnection(endpoint_id);
}

}  // namespace connections
}  // namespace nearby
