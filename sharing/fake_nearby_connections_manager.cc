// Copyright 2022 Google LLC
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

#include "sharing/fake_nearby_connections_manager.h"

#include <stdint.h>

#include <filesystem>  // NOLINT(build/c++17)
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "sharing/common/nearby_share_enums.h"
#include "sharing/internal/public/logging.h"
#include "sharing/nearby_connections_manager.h"
#include "sharing/nearby_connections_types.h"
#include "sharing/proto/enums.pb.h"

namespace nearby {
namespace sharing {

using DataUsage = ::nearby::sharing::proto::DataUsage;

FakeNearbyConnectionsManager::FakeNearbyConnectionsManager() = default;

FakeNearbyConnectionsManager::~FakeNearbyConnectionsManager() = default;

void FakeNearbyConnectionsManager::Shutdown() {
  NL_DCHECK(!IsAdvertising());
  NL_DCHECK(!IsDiscovering());
  is_shutdown_ = true;
}

void FakeNearbyConnectionsManager::StartAdvertising(
    std::vector<uint8_t> endpoint_info, IncomingConnectionListener* listener,
    PowerLevel power_level, DataUsage data_usage, bool use_stable_endpoint_id,
    ConnectionsCallback callback) {
  NL_DCHECK(!IsAdvertising());
  is_shutdown_ = false;
  {
    absl::MutexLock lock(&listener_mutex_);
    advertising_listener_ = listener;
  }
  advertising_data_usage_ = data_usage;
  advertising_power_level_ = power_level;
  advertising_endpoint_info_ = std::move(endpoint_info);
  if (capture_next_start_advertising_callback_) {
    pending_start_advertising_callback_ = std::move(callback);
    capture_next_start_advertising_callback_ = false;
  } else {
    std::move(callback)(Status::kSuccess);
  }
}

void FakeNearbyConnectionsManager::StopAdvertising(
    ConnectionsCallback callback) {
  NL_DCHECK(IsAdvertising());
  NL_DCHECK(!is_shutdown());
  {
    absl::MutexLock lock(&listener_mutex_);
    advertising_listener_ = nullptr;
  }
  advertising_data_usage_ = DataUsage::UNKNOWN_DATA_USAGE;
  advertising_power_level_ = PowerLevel::kUnknown;
  advertising_endpoint_info_.reset();
  if (capture_next_stop_advertising_callback_) {
    pending_stop_advertising_callback_ = std::move(callback);
    capture_next_stop_advertising_callback_ = false;
  } else {
    std::move(callback)(Status::kSuccess);
  }
}

void FakeNearbyConnectionsManager::StartDiscovery(
    DiscoveryListener* listener, DataUsage data_usage,
    ConnectionsCallback callback) {
  is_shutdown_ = false;
  absl::MutexLock lock(&listener_mutex_);
  discovery_listener_ = listener;
  std::move(callback)(Status::kSuccess);
}

void FakeNearbyConnectionsManager::StopDiscovery() {
  NL_DCHECK(IsDiscovering());
  NL_DCHECK(!is_shutdown());
  absl::MutexLock lock(&listener_mutex_);
  discovery_listener_ = nullptr;
}

void FakeNearbyConnectionsManager::Connect(
    std::vector<uint8_t> endpoint_info, absl::string_view endpoint_id,
    std::optional<std::vector<uint8_t>> bluetooth_mac_address,
    DataUsage data_usage, TransportType transport_type,
    NearbyConnectionCallback callback) {
  NL_DCHECK(!is_shutdown());
  connected_data_usage_ = data_usage;
  transport_type_ = transport_type;
  connection_endpoint_infos_.emplace(endpoint_id, std::move(endpoint_info));
  std::move(callback)(connection_, Status::kUnknown);
}

void FakeNearbyConnectionsManager::Disconnect(absl::string_view endpoint_id) {
  NL_DCHECK(!is_shutdown());
  connection_endpoint_infos_.erase(std::string(endpoint_id));
}

void FakeNearbyConnectionsManager::Send(
    absl::string_view endpoint_id, std::unique_ptr<Payload> payload,
    std::weak_ptr<PayloadStatusListener> listener) {
  NL_DCHECK(!is_shutdown());
  if (send_payload_callback_)
    send_payload_callback_(std::move(payload), listener);
}

void FakeNearbyConnectionsManager::RegisterPayloadStatusListener(
    int64_t payload_id, std::weak_ptr<PayloadStatusListener> listener) {
  NL_DCHECK(!is_shutdown());

  payload_status_listeners_[payload_id] = listener;
}

const Payload* FakeNearbyConnectionsManager::GetIncomingPayload(
    int64_t payload_id) const {
  NL_DCHECK(!is_shutdown());
  absl::MutexLock lock(&incoming_payloads_mutex_);
  auto it = incoming_payloads_.find(payload_id);
  if (it == incoming_payloads_.end()) return nullptr;

  return it->second.get();
}

void FakeNearbyConnectionsManager::Cancel(int64_t payload_id) {
  NL_DCHECK(!is_shutdown());
  std::weak_ptr<PayloadStatusListener> listener =
      GetRegisteredPayloadStatusListener(payload_id);
  if (auto weak_listener = listener.lock()) {
    auto status_update = std::make_unique<PayloadTransferUpdate>();
    status_update->payload_id = payload_id;
    status_update->status = PayloadStatus::kCanceled;
    status_update->total_bytes = 0;
    status_update->bytes_transferred = 0;
    weak_listener->OnStatusUpdate(std::move(status_update),
                                  /*upgraded_medium=*/std::nullopt);
    payload_status_listeners_.erase(payload_id);
  }

  canceled_payload_ids_.insert(payload_id);
}

void FakeNearbyConnectionsManager::ClearIncomingPayloads() {
  absl::MutexLock lock(&incoming_payloads_mutex_);
  incoming_payloads_.clear();
  payload_status_listeners_.clear();
}

std::optional<std::vector<uint8_t>>
FakeNearbyConnectionsManager::GetRawAuthenticationToken(
    absl::string_view endpoint_id) {
  NL_DCHECK(!is_shutdown());

  auto iter = endpoint_auth_tokens_.find(std::string(endpoint_id));
  if (iter != endpoint_auth_tokens_.end()) return iter->second;

  return std::nullopt;
}

void FakeNearbyConnectionsManager::SetRawAuthenticationToken(
    absl::string_view endpoint_id, std::vector<uint8_t> token) {
  endpoint_auth_tokens_[std::string(endpoint_id)] = std::move(token);
}

void FakeNearbyConnectionsManager::UpgradeBandwidth(
    absl::string_view endpoint_id) {
  upgrade_bandwidth_endpoint_ids_.insert(std::string(endpoint_id));
}

void FakeNearbyConnectionsManager::OnEndpointFound(
    absl::string_view endpoint_id,
    std::unique_ptr<DiscoveredEndpointInfo> info) {
  DiscoveryListener* listener = nullptr;
  {
    absl::MutexLock lock(&listener_mutex_);
    listener = discovery_listener_;
  }
  if (listener == nullptr) return;
  listener->OnEndpointDiscovered(endpoint_id, info->endpoint_info);
}

void FakeNearbyConnectionsManager::OnEndpointLost(
    absl::string_view endpoint_id) {
  DiscoveryListener* listener = nullptr;
  {
    absl::MutexLock lock(&listener_mutex_);
    listener = discovery_listener_;
  }
  if (listener == nullptr) return;
  listener->OnEndpointLost(endpoint_id);
}

bool FakeNearbyConnectionsManager::IsAdvertising() const {
  absl::MutexLock lock(&listener_mutex_);
  return advertising_listener_ != nullptr;
}

bool FakeNearbyConnectionsManager::IsDiscovering() const {
  absl::MutexLock lock(&listener_mutex_);
  return discovery_listener_ != nullptr;
}

bool FakeNearbyConnectionsManager::DidUpgradeBandwidth(
    absl::string_view endpoint_id) const {
  return upgrade_bandwidth_endpoint_ids_.find(endpoint_id) !=
         upgrade_bandwidth_endpoint_ids_.end();
}

std::weak_ptr<FakeNearbyConnectionsManager::PayloadStatusListener>
FakeNearbyConnectionsManager::GetRegisteredPayloadStatusListener(
    int64_t payload_id) {
  auto it = payload_status_listeners_.find(payload_id);
  if (it != payload_status_listeners_.end()) return it->second;

  return std::weak_ptr<FakeNearbyConnectionsManager::PayloadStatusListener>();
}

void FakeNearbyConnectionsManager::SetIncomingPayload(
    int64_t payload_id, std::unique_ptr<Payload> payload) {
  absl::MutexLock lock(&incoming_payloads_mutex_);
  incoming_payloads_[payload_id] = std::move(payload);
}

bool FakeNearbyConnectionsManager::WasPayloadCanceled(
    int64_t payload_id) const {
  return absl::c_linear_search(canceled_payload_ids_, payload_id);
}

void FakeNearbyConnectionsManager::CleanupForProcessStopped() {
  absl::MutexLock lock(&listener_mutex_);
  advertising_listener_ = nullptr;
  advertising_data_usage_ = DataUsage::UNKNOWN_DATA_USAGE;
  advertising_power_level_ = PowerLevel::kUnknown;
  advertising_endpoint_info_.reset();

  discovery_listener_ = nullptr;

  is_shutdown_ = true;
}

FakeNearbyConnectionsManager::ConnectionsCallback
FakeNearbyConnectionsManager::GetStartAdvertisingCallback() {
  capture_next_start_advertising_callback_ = true;

  FakeNearbyConnectionsManager::ConnectionsCallback callback =
      [&](ConnectionsStatus status) { HandleStartAdvertisingCallback(status); };

  return callback;
}

FakeNearbyConnectionsManager::ConnectionsCallback
FakeNearbyConnectionsManager::GetStopAdvertisingCallback() {
  capture_next_stop_advertising_callback_ = true;

  ConnectionsCallback callback = [&](ConnectionsStatus status) {
    HandleStopAdvertisingCallback(status);
  };
  return callback;
}

void FakeNearbyConnectionsManager::HandleStartAdvertisingCallback(
    ConnectionsStatus status) {
  if (pending_start_advertising_callback_) {
    std::move(pending_start_advertising_callback_)(status);
  }
  capture_next_start_advertising_callback_ = false;
}

void FakeNearbyConnectionsManager::HandleStopAdvertisingCallback(
    ConnectionsStatus status) {
  if (pending_stop_advertising_callback_) {
    std::move(pending_stop_advertising_callback_)(status);
  }
  capture_next_stop_advertising_callback_ = false;
}

void FakeNearbyConnectionsManager::SetCustomSavePath(
    absl::string_view custom_save_path) {
  custom_save_path_ = custom_save_path;
}

absl::flat_hash_set<std::filesystem::path>
FakeNearbyConnectionsManager::GetAndClearUnknownFilePathsToDelete() {
  absl::flat_hash_set<std::filesystem::path> file_paths_to_delete =
      file_paths_to_delete_;
  file_paths_to_delete_.clear();
  return file_paths_to_delete;
}

absl::flat_hash_set<std::filesystem::path>
FakeNearbyConnectionsManager::GetUnknownFilePathsToDeleteForTesting() {
  return file_paths_to_delete_;
}
void FakeNearbyConnectionsManager::AddUnknownFilePathsToDeleteForTesting(
    std::filesystem::path file_path) {
  file_paths_to_delete_.insert(file_path);
}

std::string FakeNearbyConnectionsManager::Dump() const { return ""; }

}  // namespace sharing
}  // namespace nearby
