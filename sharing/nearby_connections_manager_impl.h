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

#ifndef THIRD_PARTY_NEARBY_SHARING_NEARBY_CONNECTIONS_MANAGER_IMPL_H_
#define THIRD_PARTY_NEARBY_SHARING_NEARBY_CONNECTIONS_MANAGER_IMPL_H_

#include <stdint.h>

#include <filesystem>  // NOLINT(build/c++17)
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/string_view.h"
#include "internal/platform/device_info.h"
#include "internal/platform/mutex.h"
#include "internal/platform/task_runner.h"
#include "internal/platform/timer.h"
#include "sharing/common/nearby_share_enums.h"
#include "sharing/internal/public/connectivity_manager.h"
#include "sharing/internal/public/context.h"
#include "sharing/nearby_connection_impl.h"
#include "sharing/nearby_connections_manager.h"
#include "sharing/nearby_connections_service.h"
#include "sharing/nearby_connections_types.h"
#include "sharing/transfer_manager.h"

namespace nearby {
namespace sharing {

// Concrete NearbyConnectionsManager implementation.
class NearbyConnectionsManagerImpl : public NearbyConnectionsManager {
 public:
  explicit NearbyConnectionsManagerImpl(
      nearby::TaskRunner* connections_callback_task_runner, Context* context,
      nearby::ConnectivityManager& connectivity_manager,
      nearby::DeviceInfo& device_info,
      std::unique_ptr<NearbyConnectionsService> nearby_connections_service);
  ~NearbyConnectionsManagerImpl() override;
  NearbyConnectionsManagerImpl(const NearbyConnectionsManagerImpl&) = delete;
  NearbyConnectionsManagerImpl& operator=(const NearbyConnectionsManagerImpl&) =
      delete;

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
  const Payload* GetIncomingPayload(int64_t payload_id) const override
      ABSL_LOCKS_EXCLUDED(mutex_);
  void Cancel(int64_t payload_id) override;
  void ClearIncomingPayloads() override;
  std::optional<std::vector<uint8_t>> GetRawAuthenticationToken(
      absl::string_view endpoint_id) override;
  void UpgradeBandwidth(absl::string_view endpoint_id) override;
  void SetCustomSavePath(absl::string_view custom_save_path) override;
  absl::flat_hash_set<std::filesystem::path>
  GetAndClearUnknownFilePathsToDelete() override;
  std::string Dump() const override;

  NearbyConnectionsService* GetNearbyConnectionsService() const {
    return nearby_connections_service_.get();
  }

  absl::flat_hash_set<std::filesystem::path>
  GetUnknownFilePathsToDeleteForTesting();
  void AddUnknownFilePathsToDeleteForTesting(std::filesystem::path file_path);
  void ProcessUnknownFilePathsToDeleteForTesting(
      PayloadStatus status, PayloadContent::Type type,
      const std::filesystem::path& path);
  void OnPayloadTransferUpdateForTesting(absl::string_view endpoint_id,
                                         const PayloadTransferUpdate& update);
  void OnPayloadReceivedForTesting(absl::string_view endpoint_id,
                                   Payload& payload);

 private:
  // EndpointDiscoveryListener:
  void OnEndpointFound(absl::string_view endpoint_id,
                       const DiscoveredEndpointInfo& info);
  void OnEndpointLost(absl::string_view endpoint_id);

  // ConnectionLifecycleListener:
  void OnConnectionInitiated(absl::string_view endpoint_id,
                             const ConnectionInfo& info);
  void OnConnectionAccepted(absl::string_view endpoint_id);
  void OnConnectionRejected(absl::string_view endpoint_id, Status status);
  void OnDisconnected(absl::string_view endpoint_id);
  void OnBandwidthChanged(absl::string_view endpoint_id, Medium medium);

  // PayloadListener:
  void OnPayloadReceived(absl::string_view endpoint_id, Payload& payload);
  void OnPayloadTransferUpdate(absl::string_view endpoint_id,
                               const PayloadTransferUpdate& update);
  void OnConnectionTimedOut(absl::string_view endpoint_id);
  void OnConnectionRequested(absl::string_view endpoint_id,
                             ConnectionsStatus status);
  void ProcessUnknownFilePathsToDelete(PayloadStatus status,
                                       PayloadContent::Type type,
                                       const std::filesystem::path& path);
  void DeleteUnknownFilePayloadAndCancel(Payload& payload);
  absl::flat_hash_set<std::filesystem::path> GetUnknownFilePathsToDelete();

  std::optional<std::weak_ptr<PayloadStatusListener>> GetStatusListenerForId(
      int64_t payload_id) const ABSL_LOCKS_EXCLUDED(mutex_);

  NearbyConnectionImpl* GetConnectionForId(absl::string_view endpoint_id) const
      ABSL_LOCKS_EXCLUDED(mutex_);

  void RemoveStatusListenerForPayloadId(int64_t payload_id)
      ABSL_LOCKS_EXCLUDED(mutex_);

  void Reset();

  std::optional<Medium> GetUpgradedMedium(absl::string_view endpoint_id) const;

  void SendWithoutDelay(absl::string_view endpoint_id,
                        std::unique_ptr<Payload> payload);

  nearby::TaskRunner* const connections_callback_task_runner_;
  Context* const context_;
  nearby::ConnectivityManager& connectivity_manager_;
  nearby::DeviceInfo& device_info_;

  // Nearby Connections Manager is called from different threads and may have
  // multiple calls to the class from one thread. To avoid deadlock and access
  // violation, use a recursive mutex to protect class members.
  mutable RecursiveMutex mutex_;

  std::unique_ptr<NearbyConnectionsService> nearby_connections_service_ =
      nullptr;
  IncomingConnectionListener* incoming_connection_listener_ = nullptr;
  DiscoveryListener* discovery_listener_ = nullptr;
  absl::flat_hash_set<std::string> discovered_endpoints_
      ABSL_GUARDED_BY(mutex_);

  // A map of endpoint_id to NearbyConnectionCallback.
  absl::flat_hash_map<std::string, NearbyConnectionCallback>
      pending_outgoing_connections_ ABSL_GUARDED_BY(mutex_);

  // A map of endpoint_id to ConnectionInfoPtr.
  absl::flat_hash_map<std::string, ConnectionInfo> connection_info_map_
      ABSL_GUARDED_BY(mutex_);

  // A map of endpoint_id to NearbyConnection.
  absl::flat_hash_map<std::string, std::unique_ptr<NearbyConnectionImpl>>
      connections_ ABSL_GUARDED_BY(mutex_);

  // A map of endpoint_id to timers that timeout a connection request.
  absl::flat_hash_map<std::string, std::unique_ptr<Timer>>
      connect_timeout_timers_ ABSL_GUARDED_BY(mutex_);

  // A map of payload_id to PayloadStatusListener weak pointer.
  absl::flat_hash_map<int64_t, std::weak_ptr<PayloadStatusListener>>
      payload_status_listeners_ ABSL_GUARDED_BY(mutex_);

  // A map of payload_id to PayloadPtr.
  absl::flat_hash_map<int64_t, Payload> incoming_payloads_
      ABSL_GUARDED_BY(mutex_);

  // For metrics. A set of endpoint_ids for which we have requested a
  // bandwidth upgrade.
  absl::flat_hash_set<std::string> requested_bwu_endpoint_ids_
      ABSL_GUARDED_BY(mutex_);

  // For metrics. A map of endpoint_id to the current upgraded medium.
  absl::flat_hash_map<std::string, Medium> current_upgraded_mediums_
      ABSL_GUARDED_BY(mutex_);

  // A map of endpoint_id to transfer manager.
  absl::flat_hash_map<std::string, std::unique_ptr<TransferManager>>
      transfer_managers_ ABSL_GUARDED_BY(mutex_);

  // Avoid calling to disconnect on an endpoint multiple times.
  absl::flat_hash_set<std::string> disconnecting_endpoints_
      ABSL_GUARDED_BY(mutex_);

  // A set of file paths to delete.
  absl::flat_hash_set<std::filesystem::path> file_paths_to_delete_
      ABSL_GUARDED_BY(mutex_);
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_NEARBY_CONNECTIONS_MANAGER_IMPL_H_
