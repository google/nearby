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

#ifndef THIRD_PARTY_NEARBY_SHARING_FAKE_NEARBY_CONNECTIONS_MANAGER_H_
#define THIRD_PARTY_NEARBY_SHARING_FAKE_NEARBY_CONNECTIONS_MANAGER_H_

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

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "sharing/common/nearby_share_enums.h"
#include "sharing/nearby_connections_manager.h"
#include "sharing/nearby_connections_types.h"

namespace nearby {
namespace sharing {

// Fake NearbyConnectionsManager for testing.
class FakeNearbyConnectionsManager : public NearbyConnectionsManager {
 public:
  FakeNearbyConnectionsManager();
  ~FakeNearbyConnectionsManager() override;

  // NearbyConnectionsManager:
  void Shutdown() override;
  void StartAdvertising(std::vector<uint8_t> endpoint_info,
                        IncomingConnectionListener* listener,
                        PowerLevel power_level, proto::DataUsage data_usage,
                        bool use_stable_endpoint_id,
                        ConnectionsCallback callback) override;
  void StopAdvertising(ConnectionsCallback callback) override;
  void StartDiscovery(DiscoveryListener* listener, proto::DataUsage data_usage,
                      ConnectionsCallback callback) override;
  void StopDiscovery() override;
  void Connect(std::vector<uint8_t> endpoint_info,
               absl::string_view endpoint_id,
               std::optional<std::vector<uint8_t>> bluetooth_mac_address,
               proto::DataUsage data_usage, TransportType transport_type,
               NearbyConnectionCallback callback) override;
  void Disconnect(absl::string_view endpoint_id) override;
  void Send(absl::string_view endpoint_id, std::unique_ptr<Payload> payload,
            std::weak_ptr<PayloadStatusListener> listener) override;
  void RegisterPayloadStatusListener(
      int64_t payload_id,
      std::weak_ptr<PayloadStatusListener> listener) override;
  const Payload* GetIncomingPayload(int64_t payload_id) const override;
  void Cancel(int64_t payload_id) override;
  void ClearIncomingPayloads() override;
  std::optional<std::vector<uint8_t>> GetRawAuthenticationToken(
      absl::string_view endpoint_id) override;
  void UpgradeBandwidth(absl::string_view endpoint_id) override;
  void SetCustomSavePath(absl::string_view custom_save_path) override;
  absl::flat_hash_set<std::filesystem::path>
  GetAndClearUnknownFilePathsToDelete() override;

  // Testing methods
  void SetRawAuthenticationToken(absl::string_view endpoint_id,
                                 std::vector<uint8_t> token);

  void OnEndpointFound(absl::string_view endpoint_id,
                       std::unique_ptr<DiscoveredEndpointInfo> info);
  void OnEndpointLost(absl::string_view endpoint_id);

  bool IsAdvertising() const;
  bool IsDiscovering() const;
  bool DidUpgradeBandwidth(absl::string_view endpoint_id) const;
  std::weak_ptr<PayloadStatusListener> GetRegisteredPayloadStatusListener(
      int64_t payload_id);
  void SetIncomingPayload(int64_t payload_id, std::unique_ptr<Payload> payload);
  bool WasPayloadCanceled(int64_t payload_id) const;
  void CleanupForProcessStopped();
  ConnectionsCallback GetStartAdvertisingCallback();
  ConnectionsCallback GetStopAdvertisingCallback();

  bool is_shutdown() const { return is_shutdown_; }
  proto::DataUsage advertising_data_usage() const {
    return advertising_data_usage_;
  }
  PowerLevel advertising_power_level() const {
    return advertising_power_level_;
  }
  void set_nearby_connection(NearbyConnection* connection) {
    connection_ = connection;
  }
  proto::DataUsage connected_data_usage() const {
    return connected_data_usage_;
  }
  TransportType transport_type() const { return transport_type_; }
  void set_send_payload_callback(
      std::function<void(std::unique_ptr<Payload>,
                         std::weak_ptr<PayloadStatusListener>)>
          callback) {
    send_payload_callback_ = std::move(callback);
  }
  const std::optional<std::vector<uint8_t>>& advertising_endpoint_info() {
    return advertising_endpoint_info_;
  }

  std::optional<std::vector<uint8_t>> connection_endpoint_info(
      absl::string_view endpoint_id) {
    auto it = connection_endpoint_infos_.find(std::string(endpoint_id));
    if (it == connection_endpoint_infos_.end()) return std::nullopt;

    return it->second;
  }

  bool has_incoming_payloads() {
    absl::MutexLock lock(&incoming_payloads_mutex_);
    return !incoming_payloads_.empty();
  }

  absl::flat_hash_set<std::filesystem::path>
  GetUnknownFilePathsToDeleteForTesting();
  void AddUnknownFilePathsToDeleteForTesting(std::filesystem::path file_path);

 private:
  void HandleStartAdvertisingCallback(ConnectionsStatus status);
  void HandleStopAdvertisingCallback(ConnectionsStatus status);

  mutable absl::Mutex listener_mutex_;
  IncomingConnectionListener* advertising_listener_
      ABSL_GUARDED_BY(listener_mutex_) = nullptr;
  DiscoveryListener* discovery_listener_ ABSL_GUARDED_BY(listener_mutex_) =
      nullptr;
  bool is_shutdown_ = false;
  proto::DataUsage advertising_data_usage_ =
      proto::DataUsage::UNKNOWN_DATA_USAGE;
  PowerLevel advertising_power_level_ = PowerLevel::kUnknown;
  absl::flat_hash_set<std::string> upgrade_bandwidth_endpoint_ids_;
  std::map<std::string, std::vector<uint8_t>> endpoint_auth_tokens_;
  NearbyConnection* connection_ = nullptr;
  proto::DataUsage connected_data_usage_ = proto::DataUsage::UNKNOWN_DATA_USAGE;
  TransportType transport_type_ = TransportType::kAny;
  std::function<void(std::unique_ptr<Payload>,
                     std::weak_ptr<PayloadStatusListener>)>
      send_payload_callback_;
  std::optional<std::vector<uint8_t>> advertising_endpoint_info_;
  std::set<std::string> disconnected_endpoints_;
  std::set<int64_t> canceled_payload_ids_;
  bool capture_next_stop_advertising_callback_ = false;
  ConnectionsCallback pending_stop_advertising_callback_;
  bool capture_next_start_advertising_callback_ = false;
  ConnectionsCallback pending_start_advertising_callback_;
  std::string custom_save_path_;

  // Maps endpoint_id to endpoint_info.
  std::map<std::string, std::vector<uint8_t>> connection_endpoint_infos_;

  std::map<int64_t, std::weak_ptr<PayloadStatusListener>>
      payload_status_listeners_;
  mutable absl::Mutex incoming_payloads_mutex_;
  std::map<int64_t, std::unique_ptr<Payload>> incoming_payloads_
      ABSL_GUARDED_BY(incoming_payloads_mutex_);
  absl::flat_hash_set<std::filesystem::path> file_paths_to_delete_;
  std::string Dump() const override;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_FAKE_NEARBY_CONNECTIONS_MANAGER_H_
