// Copyright 2022-2023 Google LLC
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

#include "sharing/nearby_connections_manager_impl.h"

#include <stdint.h>

#include <functional>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/functional/bind_front.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "internal/base/file_path.h"
#include "internal/flags/nearby_flags.h"
#include "internal/platform/implementation/device_info.h"
#include "internal/platform/single_thread_executor.h"
#include "sharing/advertisement.h"
#include "sharing/common/nearby_share_enums.h"
#include "sharing/constants.h"
#include "sharing/flags/generated/nearby_sharing_feature_flags.h"
#include "sharing/internal/base/encode.h"
#include "sharing/internal/public/connectivity_manager.h"
#include "sharing/internal/public/logging.h"
#include "sharing/nearby_connection_impl.h"
#include "sharing/nearby_connections_manager.h"
#include "sharing/nearby_connections_service.h"
#include "sharing/nearby_connections_types.h"
#include "sharing/thread_timer.h"
#include "sharing/transfer_manager.h"

namespace nearby::sharing {
namespace {
using ::nearby::sharing::proto::DataUsage;

constexpr char kServiceId[] = "NearbySharing";
constexpr char kFastAdvertisementServiceUuid[] =
    "0000fef3-0000-1000-8000-00805f9b34fb";
constexpr Strategy kStrategy = Strategy::kP2pPointToPoint;

const uint8_t kMinimumAdvertisementSize =
    /* Version(3 bits)|Visibility(1 bit)|Device Type(3 bits)|Reserved(1 bits)=
     */
    1 + Advertisement::kSaltSize +
    Advertisement::kMetadataEncryptionKeyHashByteSize;

bool ShouldUseInternet(ConnectivityManager& connectivity_manager,
                       DataUsage data_usage, PowerLevel power_level) {
  // We won't use the internet if the user requested we don't.
  if (data_usage == DataUsage::OFFLINE_DATA_USAGE) return false;

  // We won't use the internet in a low power mode.
  if (power_level == PowerLevel::kLowPower) return false;

  // Verify that this network has an internet connection.
  if (!connectivity_manager.IsInternetConnected()) {
    VLOG(1) << __func__ << ": No internet connection.";
    return false;
  }

  if (data_usage == DataUsage::WIFI_ONLY_DATA_USAGE &&
      !connectivity_manager.IsLanConnected()) {
    return false;
  }

  // We're online, the user hasn't disabled Wi-Fi, let's use it!
  return true;
}

bool ShouldEnableWebRtc(ConnectivityManager& connectivity_manager,
                        DataUsage data_usage, PowerLevel power_level) {
  return NearbyFlags::GetInstance().GetBoolFlag(
             config_package_nearby::nearby_sharing_feature::
                 kEnableMediumWebRtc) &&
         ShouldUseInternet(connectivity_manager, data_usage, power_level);
}

bool ShouldEnableWifiLan(ConnectivityManager& connectivity_manager) {
  if (!NearbyFlags::GetInstance().GetBoolFlag(
          config_package_nearby::nearby_sharing_feature::
              kEnableMediumWifiLan)) {
    return false;
  }

  return connectivity_manager.IsLanConnected();
}

// Temporarily fix to get around wifi hotspot issues for HP Aero with Realtek.
// See b/380191431 for more details.

bool ShouldEnableWifiHotspot(ConnectivityManager& connectivity_manager) {
  if (connectivity_manager.IsHPRealtekDevice()) {
    LOG(WARNING) << __func__ << ": Disable wifi hotspot for HP with Realtek";
    return false;
  }
  return true;
}

bool ShouldEnableBleForTransfers() {
  return NearbyFlags::GetInstance().GetBoolFlag(
      config_package_nearby::nearby_sharing_feature::kEnableBleForTransfer);
}

std::string MediumSelectionToString(const MediumSelection& mediums) {
  std::stringstream ss;
  ss << "{";
  if (mediums.bluetooth) ss << "bluetooth ";
  if (mediums.ble) ss << "ble ";
  if (mediums.web_rtc) ss << "webrtc ";
  if (mediums.wifi_lan) ss << "wifilan ";
  if (mediums.wifi_hotspot) ss << "wifihotspot ";
  ss << "}";

  return ss.str();
}

std::string PayloadStatusToString(PayloadStatus status) {
  switch (status) {
    case PayloadStatus::kSuccess:
      return "Success";
    case PayloadStatus::kFailure:
      return "Failure";
    case PayloadStatus::kInProgress:
      return "In Progress";
    case PayloadStatus::kCanceled:
      return "Canceled";
  }
}
}  // namespace

NearbyConnectionsManagerImpl::NearbyConnectionsManagerImpl(
    SingleThreadExecutor* absl_nonnull connections_callback_task_runner,
    ConnectivityManager& connectivity_manager,
    nearby::api::DeviceInfo& device_info,
    absl_nonnull std::unique_ptr<NearbyConnectionsService>
        nearby_connections_service)
    : connections_callback_task_runner_(*connections_callback_task_runner),
      connectivity_manager_(connectivity_manager),
      device_info_(device_info),
      nearby_connections_service_(std::move(nearby_connections_service)) {}

NearbyConnectionsManagerImpl::~NearbyConnectionsManagerImpl() {
  std::shared_ptr<DiscoveryListenerState> discovery_listener;
  absl::flat_hash_map<std::string, std::shared_ptr<TransferManager>>
      transfer_managers;
  {
    absl::MutexLock lock(mutex_);
    ClearIncomingPayloadsLocked();
    discovery_listener.swap(discovery_listener_);
    transfer_managers.swap(transfer_managers_);
  }
  if (discovery_listener) {
    discovery_listener->StopDiscovery();
  }
  for (const auto& [endpoint_id, transfer_manager] : transfer_managers) {
    if (transfer_manager) {
      transfer_manager->CancelTransfer();
    }
  }
}

void NearbyConnectionsManagerImpl::Shutdown() { Reset(); }

void NearbyConnectionsManagerImpl::StartAdvertising(
    std::vector<uint8_t> endpoint_info, IncomingConnectionListener* listener,
    PowerLevel power_level, DataUsage data_usage, bool use_stable_endpoint_id,
    bool force_new_endpoint_id, ConnectionsCallback callback) {
  DCHECK(listener);

  bool is_high_power = power_level == PowerLevel::kHighPower;
  bool use_ble = true;

  MediumSelection allowed_mediums = MediumSelection(
      /*bluetooth=*/is_high_power, /*ble=*/use_ble,
      // Using kHighPower here rather than power_level to signal that power
      // level isn't a factor when deciding whether to allow WebRTC
      // upgrades from this advertisement.
      ShouldEnableWebRtc(connectivity_manager_, data_usage,
                         PowerLevel::kHighPower),
      ShouldEnableWifiLan(connectivity_manager_),
      ShouldEnableWifiHotspot(connectivity_manager_));
  VLOG(1) << __func__ << ": "
          << "is_high_power=" << (is_high_power ? "yes" : "no")
          << ", data_usage=" << static_cast<int>(data_usage)
          << ", allowed_mediums=" << MediumSelectionToString(allowed_mediums);

  // Nearby Sharing manually controls Wi-Fi/Bluetooth upgrade. Frequent
  // Bluetooth connection drops were observed during upgrades for Bluetooth
  // transfers. Android has similar logic to handle upgrades, please check
  // b/161880863 for more information.
  bool auto_upgrade_bandwidth = false;

  {
    absl::MutexLock lock(mutex_);
    DCHECK(!incoming_connection_listener_);
    incoming_connection_listener_ = listener;
  }

  Uuid fast_advertisement_service_uuid;

  LOG(INFO) << __func__ << ": Nearby Sharing flag kEnableBleV2 is enabled.";
  // Uses fast advertisement when advertisement data size is less than
  // kMinimumAdvertisementSize. Nearby Connections will decide whether to use
  // GATT server with this information.
  if (endpoint_info.size() > kMinimumAdvertisementSize) {
    fast_advertisement_service_uuid = Uuid("");
  } else {
    fast_advertisement_service_uuid = Uuid(kFastAdvertisementServiceUuid);
  }

  nearby_connections_service_->StartAdvertising(
      kServiceId, endpoint_info,
      {
          .strategy = kStrategy,
          .allowed_mediums = std::move(allowed_mediums),
          .auto_upgrade_bandwidth = auto_upgrade_bandwidth,
          .enforce_topology_constraints = true,
          .enable_bluetooth_listening = use_ble,
          .enable_webrtc_listening = ShouldEnableWebRtc(
              connectivity_manager_, data_usage, power_level),
          .use_stable_endpoint_id = use_stable_endpoint_id,
          .force_new_endpoint_id = force_new_endpoint_id,
          .fast_advertisement_service_uuid = fast_advertisement_service_uuid,
      },
      CreateConnectionListener(), std::move(callback));
}

void NearbyConnectionsManagerImpl::StopAdvertising(
    ConnectionsCallback callback) {
  {
    absl::MutexLock lock(mutex_);
    incoming_connection_listener_ = nullptr;
  }

  nearby_connections_service_->StopAdvertising(kServiceId, std::move(callback));
}

void NearbyConnectionsManagerImpl::StartDiscovery(
    DiscoveryListener* absl_nonnull listener, DataUsage data_usage,
    std::optional<uint16_t> alternate_service_uuid,
    ConnectionsCallback callback) {
  MediumSelection allowed_mediums = MediumSelection(
      /*bluetooth=*/true,
      /*ble=*/true,
      /*web_rtc=*/
      ShouldEnableWebRtc(connectivity_manager_, data_usage,
                         PowerLevel::kHighPower),
      /*wifi_lan=*/
      ShouldEnableWifiLan(connectivity_manager_),
      ShouldEnableWifiHotspot(connectivity_manager_));
  VLOG(1) << __func__ << ": "
          << "data_usage=" << static_cast<int>(data_usage)
          << ", allowed_mediums=" << MediumSelectionToString(allowed_mediums);

  auto new_discovery_listener = std::make_shared<DiscoveryListenerState>(
      connections_callback_task_runner_, listener);
  std::shared_ptr<DiscoveryListenerState> old_discovery_listener;
  {
    absl::MutexLock lock(mutex_);
    old_discovery_listener =
        std::exchange(discovery_listener_, new_discovery_listener);
  }
  if (old_discovery_listener) {
    LOG(WARNING) << "Attempting to start discovery again without stopping.";
    old_discovery_listener->StopDiscovery();
  }
  nearby_connections_service_->StartDiscovery(
      kServiceId,
      {
          .strategy = kStrategy,
          .allowed_mediums = std::move(allowed_mediums),
          .fast_advertisement_service_uuid =
              Uuid(kFastAdvertisementServiceUuid),
          .is_out_of_band_connection = false,
          .alternate_service_uuid = std::move(alternate_service_uuid),
      },
      {
          .endpoint_found_cb =
              [listener = new_discovery_listener](
                  absl::string_view endpoint_id,
                  const DiscoveredEndpointInfo& info) {
                listener->OnEndpointFound(endpoint_id, info);
              },
          .endpoint_lost_cb =
              [listener = std::move(new_discovery_listener)](
                  absl::string_view endpoint_id) {
                listener->OnEndpointLost(endpoint_id);
              },
      },
      std::move(callback));
}

void NearbyConnectionsManagerImpl::StopDiscovery() {
  std::shared_ptr<DiscoveryListenerState> discovery_listener;
  {
    absl::MutexLock lock(mutex_);
    discovery_listener.swap(discovery_listener_);
  }
  if (discovery_listener) {
    discovery_listener->StopDiscovery();
  }
  nearby_connections_service_->StopDiscovery(
      kServiceId, [](ConnectionsStatus status) {
        VLOG(1) << __func__
                << ": Stop discovery attempted over Nearby "
                   "Connections with result: "
                << ConnectionsStatusToString(status);
      });
}

std::shared_ptr<TransferManager>
NearbyConnectionsManagerImpl::CreateTransferManager(
    absl::string_view endpoint_id) {
  auto* raw_tm = new TransferManager(
      &connections_callback_task_runner_, endpoint_id,
      absl::bind_front(&NearbyConnectionsManagerImpl::SendWithoutDelay, this));
  return std::shared_ptr<TransferManager>(
      raw_tm, [runner = &connections_callback_task_runner_](
                  TransferManager* transfer_manager) {
        std::unique_ptr<TransferManager> owned(transfer_manager);
        runner->Execute([owned = std::move(owned)]() {});
      });
}

NearbyConnectionsService::ConnectionListener
NearbyConnectionsManagerImpl::CreateConnectionListener() {
  NearbyConnectionsService::ConnectionListener connection_listener;
  connection_listener.initiated_cb =
      [this](absl::string_view endpoint_id,
             const ConnectionInfo& connection_info) {
        OnConnectionInitiated(endpoint_id, connection_info);
      };
  connection_listener.accepted_cb = [this](absl::string_view endpoint_id) {
    connections_callback_task_runner_.Execute(
        [this, endpoint_id = std::string(endpoint_id)]() {
          OnConnectionAccepted(endpoint_id);
        });
  };
  connection_listener.rejected_cb = [this](absl::string_view endpoint_id,
                                           Status status) {
    connections_callback_task_runner_.Execute(
        [this, endpoint_id = std::string(endpoint_id), status]() {
          OnConnectionRejected(endpoint_id, status);
        });
  };
  connection_listener.disconnected_cb = [this](absl::string_view endpoint_id) {
    connections_callback_task_runner_.Execute(
        [this, endpoint_id = std::string(endpoint_id)]() {
          OnDisconnected(endpoint_id);
        });
  };
  connection_listener.bandwidth_changed_cb =
      [this](absl::string_view endpoint_id, Medium medium) {
        OnBandwidthChanged(endpoint_id, medium);
      };
  return connection_listener;
}

void NearbyConnectionsManagerImpl::Connect(
    std::vector<uint8_t> endpoint_info, absl::string_view endpoint_id,
    std::optional<std::vector<uint8_t>> bluetooth_mac_address,
    DataUsage data_usage, TransportType transport_type,
    NearbyConnectionCallback callback) {
  if (bluetooth_mac_address.has_value() && bluetooth_mac_address->size() != 6) {
    bluetooth_mac_address.reset();
  }
  MediumSelection allowed_mediums = MediumSelection(
      /*bluetooth=*/true,
      /*ble=*/ShouldEnableBleForTransfers(),
      ShouldEnableWebRtc(connectivity_manager_, data_usage,
                         PowerLevel::kHighPower),
      /*wifi_lan=*/
      ShouldEnableWifiLan(connectivity_manager_),
      // Note: this only affects sending since this function is only for
      // outgoing connections.
      /*wifi_hotspot=*/
      IsTransportTypeFlagsSet(transport_type, TransportType::kHighQuality) &&
          ShouldEnableWifiHotspot(connectivity_manager_));
  VLOG(1) << __func__ << ": " << "data_usage=" << static_cast<int>(data_usage)
          << ", allowed_mediums=" << MediumSelectionToString(allowed_mediums);
  std::shared_ptr<TransferManager> new_transfer_manager;
  if (IsTransportTypeFlagsSet(transport_type, TransportType::kHighQuality)) {
    new_transfer_manager = CreateTransferManager(endpoint_id);
  }
  std::shared_ptr<TransferManager> old_transfer_manager;
  {
    absl::MutexLock lock(mutex_);
    [[maybe_unused]] auto result =
        pending_outgoing_connections_.emplace(endpoint_id, std::move(callback));
    DCHECK(result.second);
    connect_timeout_timers_.insert_or_assign(
        endpoint_id,
        std::make_unique<ThreadTimer>(
            connections_callback_task_runner_, "connect_timeout_timer",
            kInitiateNearbyConnectionTimeout,
            [this, endpoint_id = std::string(endpoint_id)]() {
              OnConnectionTimedOut(endpoint_id);
            }));
    // Setup transfer manager before RequestConnection so callbacks cannot race
    // with transfer_managers_ insertion.
    if (new_transfer_manager) {
      old_transfer_manager = std::exchange(transfer_managers_[endpoint_id],
                                           std::move(new_transfer_manager));
    }
  }
  if (old_transfer_manager) {
    old_transfer_manager->CancelTransfer();
    old_transfer_manager.reset();
  }

  nearby_connections_service_->RequestConnection(
      kServiceId, endpoint_info, endpoint_id,
      {
          .allowed_mediums = std::move(allowed_mediums),
          .remote_bluetooth_mac_address = std::move(bluetooth_mac_address),
          .non_disruptive_hotspot_mode = IsTransportTypeFlagsSet(
              transport_type, TransportType::kHighQualityNonDisruptive),
      },
      CreateConnectionListener(),
      [this, endpoint_id = std::string(endpoint_id)](ConnectionsStatus status) {
        bool should_disconnect = false;
        std::shared_ptr<TransferManager> transfer_manager;
        {
          absl::MutexLock lock(mutex_);
          if (status != ConnectionsStatus::kSuccess) {
            auto node = transfer_managers_.extract(endpoint_id);
            if (!node.empty()) {
              transfer_manager = std::move(node.mapped());
            }
          }
          should_disconnect = OnConnectionRequested(endpoint_id, status);
        }
        if (transfer_manager) {
          transfer_manager->CancelTransfer();
          transfer_manager.reset();
        }
        if (should_disconnect) {
          DisconnectFromEndpoint(endpoint_id);
        }
      });
}

void NearbyConnectionsManagerImpl::OnConnectionTimedOut(
    absl::string_view endpoint_id) {
  bool should_disconnect = false;
  {
    absl::MutexLock lock(mutex_);
    LOG(ERROR) << "Failed to connect to the remote shareTarget: Timed out.";
    if (pending_outgoing_connections_.contains(endpoint_id)) {
      auto it = connection_info_map_.find(endpoint_id);
      if (it != connection_info_map_.end()) {
        it->second.connection_layer_status = Status::kTimeout;
      }
    }
    should_disconnect = StartDisconnectLocked(endpoint_id);
  }
  if (should_disconnect) {
    DisconnectFromEndpoint(endpoint_id);
  }
}

bool NearbyConnectionsManagerImpl::OnConnectionRequested(
    absl::string_view endpoint_id, ConnectionsStatus status) {
  auto it = pending_outgoing_connections_.find(endpoint_id);
  if (it == pending_outgoing_connections_.end()) return false;
  if (status != ConnectionsStatus::kSuccess) {
    LOG(ERROR) << "Failed to connect to the remote shareTarget: "
               << ConnectionsStatusToString(status);
    auto info_it = connection_info_map_.find(endpoint_id);
    if (info_it != connection_info_map_.end()) {
      info_it->second.connection_layer_status = status;
    }
    return StartDisconnectLocked(endpoint_id);
  }
  return false;
}

void NearbyConnectionsManagerImpl::Disconnect(absl::string_view endpoint_id) {
  bool should_disconnect = false;
  {
    absl::MutexLock lock(mutex_);
    should_disconnect = StartDisconnectLocked(endpoint_id);
  }
  if (should_disconnect) {
    DisconnectFromEndpoint(endpoint_id);
  }
}

bool NearbyConnectionsManagerImpl::StartDisconnectLocked(
    absl::string_view endpoint_id) {
  if (!pending_outgoing_connections_.contains(endpoint_id) &&
      !connection_info_map_.contains(endpoint_id)) {
    LOG(WARNING) << "No connection for endpoint " << endpoint_id;
    return false;
  }

  if (disconnecting_endpoints_.contains(endpoint_id)) {
    LOG(INFO) << "Another Disconnecting is running for endpoint_id "
              << endpoint_id;
    return false;
  }

  disconnecting_endpoints_.insert(std::string(endpoint_id));
  return true;
}

void NearbyConnectionsManagerImpl::DisconnectFromEndpoint(
    absl::string_view endpoint_id) {
  nearby_connections_service_->DisconnectFromEndpoint(
      kServiceId, endpoint_id,
      [this, endpoint_id = std::string(endpoint_id)](ConnectionsStatus status) {
        VLOG(1) << __func__ << ": Disconnecting from endpoint " << endpoint_id
                << " attempted over Nearby Connections with result: "
                << ConnectionsStatusToString(status);

        connections_callback_task_runner_.Execute([this, endpoint_id]() {
          OnDisconnected(endpoint_id);
          {
            absl::MutexLock lock(mutex_);
            disconnecting_endpoints_.erase(endpoint_id);
          }
        });
        LOG(INFO) << "Disconnected from " << endpoint_id;
      });
}

void NearbyConnectionsManagerImpl::Send(
    absl::string_view endpoint_id, std::unique_ptr<Payload> payload,
    std::weak_ptr<PayloadStatusListener> listener) {
  if (listener.lock()) {
    RegisterPayloadStatusListener(payload->id, listener);
  }

  if (payload->content.is_file()) {
    std::shared_ptr<TransferManager> transfer_manager;
    {
      absl::MutexLock lock(mutex_);
      const auto it = transfer_managers_.find(endpoint_id);
      if (it != transfer_managers_.end()) {
        transfer_manager = it->second;
      }
    }
    if (transfer_manager != nullptr) {
      VLOG(1) << __func__ << ": Send payload " << payload->id << " to "
              << endpoint_id << " to transfer manager. payload is file: "
              << payload->content.is_file() << ", is bytes "
              << payload->content.is_bytes();
      transfer_manager->Send(std::move(payload));
      transfer_manager->StartTransfer();
      return;
    }
  }
  SendWithoutDelay(endpoint_id, std::move(payload));
}

void NearbyConnectionsManagerImpl::SendWithoutDelay(
    absl::string_view endpoint_id, std::unique_ptr<Payload> payload) {
  VLOG(1) << __func__ << ": Send payload " << payload->id << " to "
          << endpoint_id;
  nearby_connections_service_->SendPayload(
      kServiceId, {std::string(endpoint_id)}, std::move(payload),
      [endpoint_id = std::string(endpoint_id)](ConnectionsStatus status) {
        VLOG(1) << __func__ << ": Sending payload to endpoint " << endpoint_id
                << " attempted over Nearby Connections with result: "
                << ConnectionsStatusToString(status);
      });
}

void NearbyConnectionsManagerImpl::RegisterPayloadStatusListener(
    int64_t payload_id, std::weak_ptr<PayloadStatusListener> listener) {
  absl::MutexLock lock(mutex_);
  payload_status_listeners_.insert_or_assign(payload_id, listener);
}

const Payload* NearbyConnectionsManagerImpl::GetIncomingPayload(
    int64_t payload_id) const {
  absl::MutexLock lock(mutex_);
  const auto it = incoming_payloads_.find(payload_id);
  if (it == incoming_payloads_.end()) return nullptr;

  return it->second.get();
}

void NearbyConnectionsManagerImpl::Cancel(int64_t payload_id) {
  std::weak_ptr<PayloadStatusListener> listener;
  {
    absl::MutexLock lock(mutex_);
    auto it = payload_status_listeners_.find(payload_id);
    if (it != payload_status_listeners_.end()) {
      listener = it->second;
      payload_status_listeners_.erase(it);
    }
  }

  // Note: The listener might be invalidated, for example, if it is shared
  // with another payload in the same transfer.
  if (auto status_listener = listener.lock()) {
    status_listener->OnStatusUpdate(std::make_unique<PayloadTransferUpdate>(
        payload_id, PayloadStatus::kCanceled,
        /*total_bytes=*/0,
        /*bytes_transferred=*/0));
  }

  nearby_connections_service_->CancelPayload(
      kServiceId, payload_id, [payload_id](ConnectionsStatus status) {
        VLOG(1) << __func__ << ": Cancelling payload to id " << payload_id
                << " attempted over Nearby Connections with result: "
                << ConnectionsStatusToString(status);
      });

  LOG(INFO) << "Cancelling payload: " << payload_id;
}

void NearbyConnectionsManagerImpl::ClearIncomingPayloads() {
  absl::MutexLock lock(mutex_);
  ClearIncomingPayloadsLocked();
}

void NearbyConnectionsManagerImpl::ClearIncomingPayloadsLocked() {
  incoming_payloads_.clear();
  payload_status_listeners_.clear();
}

std::optional<std::vector<uint8_t>>
NearbyConnectionsManagerImpl::GetRawAuthenticationToken(
    absl::string_view endpoint_id) {
  absl::MutexLock lock(mutex_);
  auto it = connection_info_map_.find(endpoint_id);
  if (it == connection_info_map_.end()) return std::nullopt;

  return it->second.raw_authentication_token;
}

void NearbyConnectionsManagerImpl::UpgradeBandwidth(
    absl::string_view endpoint_id) {
  // The only bandwidth upgrade mediums at this point are WebRTC and WifiLan.
  if (!NearbyFlags::GetInstance().GetBoolFlag(
          config_package_nearby::nearby_sharing_feature::
              kEnableMediumWifiLan) &&
      !NearbyFlags::GetInstance().GetBoolFlag(
          config_package_nearby::nearby_sharing_feature::kEnableMediumWebRtc)) {
    return;
  }

  {
    absl::MutexLock lock(mutex_);
    requested_bwu_endpoint_ids_.emplace(endpoint_id);
  }
  nearby_connections_service_->InitiateBandwidthUpgrade(
      kServiceId, endpoint_id,
      [endpoint_id = std::string(endpoint_id)](ConnectionsStatus status) {
        VLOG(1) << __func__ << ": Bandwidth upgrade attempted to endpoint "
                << endpoint_id << "over Nearby Connections with result: "
                << ConnectionsStatusToString(status);
      });
}

NearbyConnectionsManagerImpl::DiscoveryListenerState::DiscoveryListenerState(
    SingleThreadExecutor& task_runner, DiscoveryListener* absl_nonnull listener)
    : task_runner_(task_runner), listener_(listener) {}

void NearbyConnectionsManagerImpl::DiscoveryListenerState::StopDiscovery() {
  absl::MutexLock lock(state_mutex_);
  listener_ = nullptr;
  discovered_endpoints_.clear();
}

void NearbyConnectionsManagerImpl::DiscoveryListenerState::OnEndpointFound(
    absl::string_view endpoint_id, const DiscoveredEndpointInfo& info) {
  task_runner_.Execute([state = shared_from_this(),
                        endpoint_id = std::string(endpoint_id), info]() {
    DiscoveryListener* listener = nullptr;
    {
      absl::MutexLock lock(state->state_mutex_);
      if (state->listener_ == nullptr) {
        VLOG(1) << "Ignoring discovered endpoint "
                << nearby::utils::HexEncode(info.endpoint_info)
                << " because we're no longer "
                   "in discovery mode";
        return;
      }

      auto result = state->discovered_endpoints_.insert(endpoint_id);
      if (!result.second) {
        VLOG(1) << "Ignoring discovered endpoint "
                << nearby::utils::HexEncode(info.endpoint_info)
                << " because we've already "
                   "reported this endpoint";
        return;
      }
      listener = state->listener_;
    }

    listener->OnEndpointDiscovered(endpoint_id, info.endpoint_info);
    LOG(INFO) << "Discovered " << nearby::utils::HexEncode(info.endpoint_info)
              << " over Nearby Connections";
  });
}

void NearbyConnectionsManagerImpl::DiscoveryListenerState::OnEndpointLost(
    absl::string_view endpoint_id) {
  task_runner_.Execute([state = shared_from_this(),
                        endpoint_id = std::string(endpoint_id)]() {
    DiscoveryListener* listener = nullptr;
    {
      absl::MutexLock lock(state->state_mutex_);
      if (state->listener_ == nullptr) {
        VLOG(1) << "Ignoring lost endpoint " << endpoint_id
                << " because we're no longer in discovery mode";
        return;
      }
      if (!state->discovered_endpoints_.erase(endpoint_id)) {
        VLOG(1) << "Ignoring lost endpoint " << endpoint_id
                << " because we haven't reported this endpoint";
        return;
      }
      listener = state->listener_;
    }

    listener->OnEndpointLost(endpoint_id);
    LOG(INFO) << "Endpoint " << endpoint_id << " lost over Nearby Connections";
  });
}

void NearbyConnectionsManagerImpl::OnConnectionInitiated(
    absl::string_view endpoint_id, const ConnectionInfo& info) {
  {
    absl::MutexLock lock(mutex_);
    [[maybe_unused]] auto result =
        connection_info_map_.emplace(endpoint_id, info);
    DCHECK(result.second);
  }

  NearbyConnectionsService::PayloadListener payload_listener;

  payload_listener.payload_cb = [this](absl::string_view endpoint_id,
                                       Payload payload) {
    OnPayloadReceived(endpoint_id, payload);
  };

  payload_listener.payload_progress_cb = absl::bind_front(
      &NearbyConnectionsManagerImpl::OnPayloadTransferUpdate, this);

  nearby_connections_service_->AcceptConnection(
      kServiceId, endpoint_id, std::move(payload_listener),
      [endpoint_id = std::string(endpoint_id)](ConnectionsStatus status) {
        VLOG(1) << __func__ << ": Accept connection attempted to endpoint "
                << endpoint_id << " over Nearby Connections with result: "
                << ConnectionsStatusToString(status);
      });
}

void NearbyConnectionsManagerImpl::OnConnectionAccepted(
    absl::string_view endpoint_id) {
  IncomingConnectionListener* incoming_listener = nullptr;
  std::vector<uint8_t> endpoint_info;
  NearbyConnectionCallback outgoing_callback;
  NearbyConnectionImpl* connection_ptr = nullptr;
  bool should_disconnect = false;

  {
    absl::MutexLock lock(mutex_);
    auto it = connection_info_map_.find(endpoint_id);
    if (it == connection_info_map_.end()) return;

    if (it->second.is_incoming_connection) {
      if (!incoming_connection_listener_) {
        // Not in advertising mode.
        should_disconnect = StartDisconnectLocked(endpoint_id);
      } else {
        incoming_listener = incoming_connection_listener_;
        endpoint_info = it->second.endpoint_info;
        auto result = connections_.emplace(
            std::string(endpoint_id),
            std::make_unique<NearbyConnectionImpl>(device_info_));
        DCHECK(result.second);
        connection_ptr = result.first->second.get();
      }
    } else {
      auto outgoing_it = pending_outgoing_connections_.find(endpoint_id);
      if (outgoing_it == pending_outgoing_connections_.end()) {
        should_disconnect = StartDisconnectLocked(endpoint_id);
      } else {
        auto result = connections_.emplace(
            std::string(endpoint_id),
            std::make_unique<NearbyConnectionImpl>(device_info_));
        DCHECK(result.second);
        connection_ptr = result.first->second.get();
        outgoing_callback = std::move(outgoing_it->second);
        pending_outgoing_connections_.erase(outgoing_it);
        connect_timeout_timers_.erase(endpoint_id);
      }
    }
  }

  if (should_disconnect) {
    DisconnectFromEndpoint(endpoint_id);
    return;
  }
  if (incoming_listener != nullptr) {
    incoming_listener->OnIncomingConnection(endpoint_id, endpoint_info,
                                            connection_ptr);
  } else if (outgoing_callback) {
    std::move(outgoing_callback)(endpoint_id, connection_ptr, Status::kSuccess);
  }
}

void NearbyConnectionsManagerImpl::OnConnectionRejected(
    absl::string_view endpoint_id, Status status) {
  NearbyConnectionCallback outgoing_callback;
  std::shared_ptr<TransferManager> transfer_manager;
  {
    absl::MutexLock lock(mutex_);
    auto tm_node = transfer_managers_.extract(endpoint_id);
    if (!tm_node.empty()) {
      transfer_manager = std::move(tm_node.mapped());
    }
    connection_info_map_.erase(endpoint_id);
    requested_bwu_endpoint_ids_.erase(endpoint_id);
    current_upgraded_mediums_.erase(endpoint_id);

    auto it = pending_outgoing_connections_.find(endpoint_id);
    if (it != pending_outgoing_connections_.end()) {
      outgoing_callback = std::move(it->second);
      pending_outgoing_connections_.erase(it);
      connect_timeout_timers_.erase(endpoint_id);
    }
  }

  if (transfer_manager != nullptr) {
    transfer_manager->CancelTransfer();
    transfer_manager.reset();
  }
  if (outgoing_callback) {
    std::move(outgoing_callback)(endpoint_id, nullptr, status);
  }
}

void NearbyConnectionsManagerImpl::OnDisconnected(
    absl::string_view endpoint_id) {
  NearbyConnectionCallback outgoing_callback;
  Status connection_layer_status = Status::kUnknown;
  std::shared_ptr<TransferManager> transfer_manager;
  std::unique_ptr<NearbyConnectionImpl> connection;

  {
    absl::MutexLock lock(mutex_);
    auto tm_node = transfer_managers_.extract(endpoint_id);
    if (!tm_node.empty()) {
      transfer_manager = std::move(tm_node.mapped());
    }

    auto info_it = connection_info_map_.find(endpoint_id);
    if (info_it != connection_info_map_.end()) {
      connection_layer_status = info_it->second.connection_layer_status;
      connection_info_map_.erase(info_it);
    }

    auto it = pending_outgoing_connections_.find(endpoint_id);
    if (it != pending_outgoing_connections_.end()) {
      outgoing_callback = std::move(it->second);
      pending_outgoing_connections_.erase(it);
      connect_timeout_timers_.erase(endpoint_id);
    }

    auto conn_node = connections_.extract(endpoint_id);
    if (!conn_node.empty()) {
      connection = std::move(conn_node.mapped());
    }

    requested_bwu_endpoint_ids_.erase(endpoint_id);
    current_upgraded_mediums_.erase(endpoint_id);
  }

  if (transfer_manager != nullptr) {
    transfer_manager->CancelTransfer();
    transfer_manager.reset();
  }
  if (outgoing_callback) {
    std::move(outgoing_callback)(endpoint_id, nullptr, connection_layer_status);
  }
  connection.reset();
}

void NearbyConnectionsManagerImpl::OnBandwidthChanged(
    absl::string_view endpoint_id, Medium medium) {
  VLOG(1) << __func__
          << ": Bandwidth changed to medium=" << static_cast<int>(medium)
          << "; endpoint_id=" << endpoint_id;

  std::shared_ptr<TransferManager> transfer_manager;
  {
    absl::MutexLock lock(mutex_);
    const auto transfer_manager_it = transfer_managers_.find(endpoint_id);
    if (transfer_manager_it != transfer_managers_.end()) {
      transfer_manager = transfer_manager_it->second;
    }
    current_upgraded_mediums_.insert_or_assign(endpoint_id, medium);
  }

  if (transfer_manager != nullptr) {
    transfer_manager->OnMediumQualityChanged(medium);
  }
  // TODO(crbug/1111458): Support TransferManager.
}

void NearbyConnectionsManagerImpl::OnPayloadReceived(
    absl::string_view endpoint_id, Payload& payload) {
  VLOG(1) << "Received payload id=" << payload.id;
  bool should_cancel = false;
  {
    absl::MutexLock lock(mutex_);
    if (payload.content.type != PayloadContent::Type::kBytes) {
      auto it = payload_status_listeners_.find(payload.id);
      if (it == payload_status_listeners_.end() || it->second.expired()) {
        LOG(WARNING) << __func__ << ": Received unknown payload. Canceling.";
        should_cancel = true;
      }
    }
    if (!should_cancel) {
      int64_t payload_id = payload.id;
      if (!incoming_payloads_.contains(payload_id)) {
        incoming_payloads_.emplace(
            payload_id, std::make_unique<Payload>(std::move(payload)));
        return;
      }
      LOG(WARNING) << __func__ << ": Payload id already exists. Canceling.";
      should_cancel = true;
    }
  }
  if (should_cancel) {
    DeleteUnknownFilePayloadAndCancel(payload);
  }
}

void NearbyConnectionsManagerImpl::DeleteUnknownFilePayloadAndCancel(
    Payload& payload) {
  if (payload.content.type == PayloadContent::Type::kFile) {
    absl::MutexLock lock(file_paths_to_delete_mutex_);
    file_paths_to_delete_.insert(payload.content.file_payload.file_path);
  }
  Cancel(payload.id);
}

void NearbyConnectionsManagerImpl::OnPayloadTransferUpdate(
    absl::string_view endpoint_id, const PayloadTransferUpdate& update) {
  VLOG(1) << "Received payload transfer update id=" << update.payload_id
          << ",status=" << PayloadStatusToString(update.status)
          << ",total=" << update.total_bytes
          << ",bytes_transferred=" << update.bytes_transferred;

  // Check registered PayloadStatusListeners and incoming control frames in a
  // single lock scope to avoid TOCTOU races with RegisterPayloadStatusListener.
  std::optional<std::weak_ptr<PayloadStatusListener>> listener;
  std::optional<std::vector<uint8_t>> control_frame_bytes;
  {
    absl::MutexLock lock(mutex_);
    auto listener_it = payload_status_listeners_.find(update.payload_id);
    if (listener_it != payload_status_listeners_.end()) {
      listener = listener_it->second;
      switch (update.status) {
        case PayloadStatus::kInProgress:
          break;
        case PayloadStatus::kSuccess:
        case PayloadStatus::kCanceled:
        case PayloadStatus::kFailure:
          payload_status_listeners_.erase(listener_it);
          break;
      }
    } else if (update.status != PayloadStatus::kInProgress) {
      // If this is an incoming payload that we have not registered for, then
      // we'll treat it as a control frame (e.g. IntroductionFrame) and
      // forward it to the associated NearbyConnection.
      auto node = incoming_payloads_.extract(update.payload_id);
      if (!node.empty() && update.status == PayloadStatus::kSuccess &&
          node.mapped()->content.is_bytes()) {
        control_frame_bytes =
            std::move(node.mapped()->content.bytes_payload.bytes);
      }
    }
  }

  if (listener.has_value()) {
    // Note: The listener might be invalidated, for example, if it is shared
    // with another payload in the same transfer.
    if (auto status_listener = listener->lock()) {
      status_listener->OnStatusUpdate(
          std::make_unique<PayloadTransferUpdate>(update));
    }
    return;
  }

  if (control_frame_bytes.has_value()) {
    connections_callback_task_runner_.Execute(
        [this, endpoint_id = std::string(endpoint_id),
         bytes = std::move(*control_frame_bytes)]() mutable {
          NearbyConnectionImpl* connection_ptr = nullptr;
          {
            absl::MutexLock lock(mutex_);
            auto connection_it = connections_.find(endpoint_id);
            if (connection_it == connections_.end()) return;
            connection_ptr = connection_it->second.get();
          }
          VLOG(1) << "Writing incoming byte message to NearbyConnection.";
          connection_ptr->WriteMessage(std::move(bytes));
        });
  }
}

void NearbyConnectionsManagerImpl::Reset() {
  absl::flat_hash_map<std::string, std::unique_ptr<NearbyConnectionImpl>>
      connections;
  absl::flat_hash_map<std::string, ConnectionInfo> connection_info_map;
  std::shared_ptr<DiscoveryListenerState> discovery_listener;
  absl::flat_hash_map<std::string, std::shared_ptr<TransferManager>>
      transfer_managers;
  absl::flat_hash_map<std::string, NearbyConnectionCallback>
      pending_outgoing_connections;

  {
    absl::MutexLock lock(mutex_);
    connections.swap(connections_);
    ClearIncomingPayloadsLocked();
    connection_info_map.swap(connection_info_map_);
    discovery_listener.swap(discovery_listener_);
    incoming_connection_listener_ = nullptr;
    connect_timeout_timers_.clear();
    requested_bwu_endpoint_ids_.clear();
    current_upgraded_mediums_.clear();
    disconnecting_endpoints_.clear();
    transfer_managers.swap(transfer_managers_);
    pending_outgoing_connections.swap(pending_outgoing_connections_);
  }

  if (discovery_listener) {
    discovery_listener->StopDiscovery();
  }

  for (const auto& [endpoint_id, transfer_manager] : transfer_managers) {
    transfer_manager->CancelTransfer();
  }
  transfer_managers.clear();

  absl::flat_hash_set<std::string> endpoints_to_disconnect;
  for (const auto& [endpoint_id, connection] : connections) {
    endpoints_to_disconnect.insert(endpoint_id);
  }
  for (const auto& [endpoint_id, callback] : pending_outgoing_connections) {
    endpoints_to_disconnect.insert(endpoint_id);
  }
  for (const auto& [endpoint_id, info] : connection_info_map) {
    endpoints_to_disconnect.insert(endpoint_id);
  }
  for (const auto& endpoint_id : endpoints_to_disconnect) {
    nearby_connections_service_->DisconnectFromEndpoint(
        kServiceId, endpoint_id, [endpoint_id](ConnectionsStatus status) {
          VLOG(1) << __func__
                  << ": Disconnect from endpoint attempted over Nearby "
                     "Connections with result: "
                  << ConnectionsStatusToString(status);
        });
  }

  for (auto& entry : pending_outgoing_connections) {
    std::move(entry.second)(entry.first, /*connection=*/nullptr,
                            Status::kReset);
  }
  connections.clear();
}

std::optional<Medium> NearbyConnectionsManagerImpl::GetUpgradedMedium(
    absl::string_view endpoint_id) const {
  absl::MutexLock lock(mutex_);
  const auto it = current_upgraded_mediums_.find(endpoint_id);
  if (it == current_upgraded_mediums_.end()) return std::nullopt;

  return it->second;
}

void NearbyConnectionsManagerImpl::SetCustomSavePath(
    absl::string_view custom_save_path) {
  nearby_connections_service_->SetCustomSavePath(
      custom_save_path, [](Status status) {
        VLOG(1) << __func__
                << ": SetCustomSavePath attempted over Nearby "
                   "Connections with result: "
                << static_cast<int>(status);
      });
}

void NearbyConnectionsManagerImpl::OverrideSavePath(
    absl::string_view endpoint_id, const FilePath& custom_save_path) {
  nearby_connections_service_->OverrideSavePath(endpoint_id,
                                                custom_save_path.ToString());
}

absl::flat_hash_set<FilePath>
NearbyConnectionsManagerImpl::GetAndClearUnknownFilePathsToDelete() {
  absl::MutexLock lock(file_paths_to_delete_mutex_);
  absl::flat_hash_set<FilePath> result;
  result.swap(file_paths_to_delete_);
  return result;
}

absl::flat_hash_set<FilePath>
NearbyConnectionsManagerImpl::GetUnknownFilePathsToDeleteForTesting() {
  absl::MutexLock lock(file_paths_to_delete_mutex_);
  return file_paths_to_delete_;
}

void NearbyConnectionsManagerImpl::AddUnknownFilePathsToDeleteForTesting(
    FilePath file_path) {
  absl::MutexLock lock(file_paths_to_delete_mutex_);
  file_paths_to_delete_.insert(file_path);
}

void NearbyConnectionsManagerImpl::OnPayloadTransferUpdateForTesting(
    absl::string_view endpoint_id, const PayloadTransferUpdate& update) {
  OnPayloadTransferUpdate(endpoint_id, update);
}

void NearbyConnectionsManagerImpl::OnPayloadReceivedForTesting(
    absl::string_view endpoint_id, Payload& payload) {
  OnPayloadReceived(endpoint_id, payload);
}

std::string NearbyConnectionsManagerImpl::Dump() const {
  return nearby_connections_service_->Dump();
}

}  // namespace nearby::sharing
