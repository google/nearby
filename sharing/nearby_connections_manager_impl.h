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

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "internal/base/file_path.h"
#include "internal/platform/implementation/device_info.h"
#include "internal/platform/task_runner.h"
#include "sharing/common/nearby_share_enums.h"
#include "sharing/internal/public/connectivity_manager.h"
#include "sharing/nearby_connection_impl.h"
#include "sharing/nearby_connections_manager.h"
#include "sharing/nearby_connections_service.h"
#include "sharing/nearby_connections_types.h"
#include "sharing/thread_timer.h"
#include "sharing/transfer_manager.h"

namespace nearby {
namespace sharing {

// Concrete NearbyConnectionsManager implementation.
class NearbyConnectionsManagerImpl : public NearbyConnectionsManager {
 public:
  explicit NearbyConnectionsManagerImpl(
      nearby::TaskRunner* absl_nonnull connections_callback_task_runner,
      nearby::ConnectivityManager& connectivity_manager,
      nearby::api::DeviceInfo& device_info,
      absl_nonnull std::unique_ptr<NearbyConnectionsService>
          nearby_connections_service);
  ~NearbyConnectionsManagerImpl() override;
  NearbyConnectionsManagerImpl(const NearbyConnectionsManagerImpl&) = delete;
  NearbyConnectionsManagerImpl& operator=(const NearbyConnectionsManagerImpl&) =
      delete;

  // NearbyConnectionsManager:
  void Shutdown() override ABSL_LOCKS_EXCLUDED(mutex_);
  void StartAdvertising(std::vector<uint8_t> endpoint_info,
                        IncomingConnectionListener* listener,
                        PowerLevel power_level, proto::DataUsage data_usage,
                        bool use_stable_endpoint_id, bool force_new_endpoint_id,
                        ConnectionsCallback callback) override
      ABSL_LOCKS_EXCLUDED(mutex_);
  void StopAdvertising(ConnectionsCallback callback) override
      ABSL_LOCKS_EXCLUDED(mutex_);
  void StartDiscovery(DiscoveryListener* absl_nonnull listener,
                      proto::DataUsage data_usage,
                      std::optional<uint16_t> alternate_service_uuid,
                      ConnectionsCallback callback) override
      ABSL_LOCKS_EXCLUDED(mutex_);
  void StopDiscovery() override ABSL_LOCKS_EXCLUDED(mutex_);
  void Connect(std::vector<uint8_t> endpoint_info,
               absl::string_view endpoint_id,
               std::optional<std::vector<uint8_t>> bluetooth_mac_address,
               proto::DataUsage data_usage, TransportType transport_type,
               NearbyConnectionCallback callback) override
      ABSL_LOCKS_EXCLUDED(mutex_);
  void Disconnect(absl::string_view endpoint_id) override
      ABSL_LOCKS_EXCLUDED(mutex_);
  void Send(absl::string_view endpoint_id, std::unique_ptr<Payload> payload,
            std::weak_ptr<PayloadStatusListener> listener) override
      ABSL_LOCKS_EXCLUDED(mutex_);
  void RegisterPayloadStatusListener(
      int64_t payload_id,
      std::weak_ptr<PayloadStatusListener> listener) override
      ABSL_LOCKS_EXCLUDED(mutex_);
  const Payload* GetIncomingPayload(int64_t payload_id) const override
      ABSL_LOCKS_EXCLUDED(mutex_);
  void Cancel(int64_t payload_id) override ABSL_LOCKS_EXCLUDED(mutex_);
  void ClearIncomingPayloads() override ABSL_LOCKS_EXCLUDED(mutex_);
  std::optional<std::vector<uint8_t>> GetRawAuthenticationToken(
      absl::string_view endpoint_id) override ABSL_LOCKS_EXCLUDED(mutex_);
  void UpgradeBandwidth(absl::string_view endpoint_id) override
      ABSL_LOCKS_EXCLUDED(mutex_);
  void SetCustomSavePath(absl::string_view custom_save_path) override;
  void OverrideSavePath(absl::string_view endpoint_id,
                        const FilePath& custom_save_path) override;
  absl::flat_hash_set<FilePath> GetAndClearUnknownFilePathsToDelete() override
      ABSL_LOCKS_EXCLUDED(file_paths_to_delete_mutex_);
  std::string Dump() const override;

  NearbyConnectionsService* absl_nonnull GetNearbyConnectionsService() const {
    return nearby_connections_service_.get();
  }

  absl::flat_hash_set<FilePath> GetUnknownFilePathsToDeleteForTesting()
      ABSL_LOCKS_EXCLUDED(file_paths_to_delete_mutex_);
  void AddUnknownFilePathsToDeleteForTesting(FilePath file_path)
      ABSL_LOCKS_EXCLUDED(file_paths_to_delete_mutex_);
  void OnPayloadTransferUpdateForTesting(absl::string_view endpoint_id,
                                         const PayloadTransferUpdate& update)
      ABSL_LOCKS_EXCLUDED(mutex_);
  void OnPayloadReceivedForTesting(absl::string_view endpoint_id,
                                   Payload& payload)
      ABSL_LOCKS_EXCLUDED(mutex_, file_paths_to_delete_mutex_);

 private:
  class DiscoveryListenerState
      : public std::enable_shared_from_this<DiscoveryListenerState> {
   public:
    DiscoveryListenerState(nearby::TaskRunner& task_runner,
                           DiscoveryListener* absl_nonnull listener);

    void StopDiscovery() ABSL_LOCKS_EXCLUDED(state_mutex_);
    void OnEndpointFound(absl::string_view endpoint_id,
                         const DiscoveredEndpointInfo& info)
        ABSL_LOCKS_EXCLUDED(state_mutex_);
    void OnEndpointLost(absl::string_view endpoint_id)
        ABSL_LOCKS_EXCLUDED(state_mutex_);

   private:
    nearby::TaskRunner& task_runner_;
    absl::Mutex state_mutex_;
    DiscoveryListener* listener_ ABSL_GUARDED_BY(state_mutex_);
    absl::flat_hash_set<std::string> discovered_endpoints_
        ABSL_GUARDED_BY(state_mutex_);
  };

  // ConnectionLifecycleListener:
  void OnConnectionInitiated(absl::string_view endpoint_id,
                             const ConnectionInfo& info)
      ABSL_LOCKS_EXCLUDED(mutex_);
  void OnConnectionAccepted(absl::string_view endpoint_id)
      ABSL_LOCKS_EXCLUDED(mutex_);
  void OnConnectionRejected(absl::string_view endpoint_id, Status status)
      ABSL_LOCKS_EXCLUDED(mutex_);
  void OnDisconnected(absl::string_view endpoint_id)
      ABSL_LOCKS_EXCLUDED(mutex_);
  void OnBandwidthChanged(absl::string_view endpoint_id, Medium medium)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // PayloadListener:
  void OnPayloadReceived(absl::string_view endpoint_id, Payload& payload)
      ABSL_LOCKS_EXCLUDED(mutex_, file_paths_to_delete_mutex_);
  void OnPayloadTransferUpdate(absl::string_view endpoint_id,
                               const PayloadTransferUpdate& update)
      ABSL_LOCKS_EXCLUDED(mutex_);
  void OnConnectionTimedOut(absl::string_view endpoint_id)
      ABSL_LOCKS_EXCLUDED(mutex_);
  bool OnConnectionRequested(absl::string_view endpoint_id,
                             ConnectionsStatus status)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  void DeleteUnknownFilePayloadAndCancel(Payload& payload)
      ABSL_LOCKS_EXCLUDED(mutex_, file_paths_to_delete_mutex_);

  void Reset() ABSL_LOCKS_EXCLUDED(mutex_);

  std::optional<Medium> GetUpgradedMedium(absl::string_view endpoint_id) const
      ABSL_LOCKS_EXCLUDED(mutex_);

  void SendWithoutDelay(absl::string_view endpoint_id,
                        std::unique_ptr<Payload> payload);

  std::shared_ptr<TransferManager> CreateTransferManager(
      absl::string_view endpoint_id);
  // Creates a ConnectionListener that dispatches `OnConnectionInitiated` and
  // `OnBandwidthChanged` synchronously, and marshals accepted, rejected, and
  // disconnected lifecycle events onto `connections_callback_task_runner_`.
  NearbyConnectionsService::ConnectionListener CreateConnectionListener();

  bool StartDisconnectLocked(absl::string_view endpoint_id)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  void DisconnectFromEndpoint(absl::string_view endpoint_id)
      ABSL_LOCKS_EXCLUDED(mutex_);
  void ClearIncomingPayloadsLocked() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  nearby::TaskRunner& connections_callback_task_runner_;
  nearby::ConnectivityManager& connectivity_manager_;
  nearby::api::DeviceInfo& device_info_;
  const absl_nonnull std::unique_ptr<NearbyConnectionsService>
      nearby_connections_service_;

  mutable absl::Mutex mutex_;

  IncomingConnectionListener* incoming_connection_listener_
      ABSL_GUARDED_BY(mutex_) = nullptr;

  std::shared_ptr<DiscoveryListenerState> discovery_listener_
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
  absl::flat_hash_map<std::string, std::unique_ptr<ThreadTimer>>
      connect_timeout_timers_ ABSL_GUARDED_BY(mutex_);

  // A map of payload_id to PayloadStatusListener weak pointer.
  absl::flat_hash_map<int64_t, std::weak_ptr<PayloadStatusListener>>
      payload_status_listeners_ ABSL_GUARDED_BY(mutex_);

  // A map of payload_id to a Payload.
  absl::flat_hash_map<int64_t, std::unique_ptr<Payload>> incoming_payloads_
      ABSL_GUARDED_BY(mutex_);

  // For metrics. A set of endpoint_ids for which we have requested a
  // bandwidth upgrade.
  absl::flat_hash_set<std::string> requested_bwu_endpoint_ids_
      ABSL_GUARDED_BY(mutex_);

  // For metrics. A map of endpoint_id to the current upgraded medium.
  absl::flat_hash_map<std::string, Medium> current_upgraded_mediums_
      ABSL_GUARDED_BY(mutex_);

  // A map of endpoint_id to transfer manager.
  absl::flat_hash_map<std::string, std::shared_ptr<TransferManager>>
      transfer_managers_ ABSL_GUARDED_BY(mutex_);

  // Avoid calling to disconnect on an endpoint multiple times.
  absl::flat_hash_set<std::string> disconnecting_endpoints_
      ABSL_GUARDED_BY(mutex_);

  absl::Mutex file_paths_to_delete_mutex_;
  // A set of file paths to delete.
  absl::flat_hash_set<FilePath> file_paths_to_delete_
      ABSL_GUARDED_BY(file_paths_to_delete_mutex_);
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_NEARBY_CONNECTIONS_MANAGER_IMPL_H_
