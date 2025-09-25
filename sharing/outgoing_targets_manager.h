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

#ifndef THIRD_PARTY_NEARBY_SHARING_OUTGOING_TARGETS_MANAGER_H_
#define THIRD_PARTY_NEARBY_SHARING_OUTGOING_TARGETS_MANAGER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "internal/platform/clock.h"
#include "internal/platform/task_runner.h"
#include "proto/sharing_enums.pb.h"
#include "sharing/analytics/analytics_recorder.h"
#include "sharing/certificates/nearby_share_decrypted_public_certificate.h"
#include "sharing/nearby_connections_manager.h"
#include "sharing/outgoing_share_session.h"
#include "sharing/proto/enums.pb.h"
#include "sharing/proto/wire_format.pb.h"
#include "sharing/share_target.h"
#include "sharing/thread_timer.h"
#include "sharing/transfer_metadata.h"

namespace nearby::sharing {
class NearbyShareContactManager;

namespace NearbySharingServiceUnitTests {
class NearbySharingServiceImplTest_CreateShareTarget_Test;
class NearbySharingServiceImplTest_RemoveIncomingPayloads_Test;
};  // namespace NearbySharingServiceUnitTests

class OutgoingTargetsManager {
 public:
  OutgoingTargetsManager(
      Clock* clock, TaskRunner* service_thread,
      NearbyConnectionsManager* connections_manager,
      analytics::AnalyticsRecorder* analytics_recorder,
      absl::AnyInvocable<void(const ShareTarget&)>
          share_target_updated_callback,
      absl::AnyInvocable<void(const ShareTarget&)> share_target_lost_callback);

  void Cleanup();

  OutgoingShareSession* GetOutgoingShareSession(int64_t share_target_id);

  // Update the entry in outgoing_share_session_map_ with the new share target
  // and OnShareTargetUpdated is called.
  void DeduplicateInOutgoingShareTarget(
      const ShareTarget& share_target, absl::string_view endpoint_id,
      std::optional<NearbyShareDecryptedPublicCertificate> certificate);

      // Looks for a duplicate of the share target in the discovery cache.
  // If found, the share target is removed from the discovery cache and its
  // id is copied into `share_target`.
  // Returns true if the duplicate is found.
  bool FindDuplicateInDiscoveryCache(absl::string_view endpoint_id,
                                     ShareTarget& share_target);

  // Looks for a duplicate of the share target in the outgoing share
  // target map. The share target's id is changed to match an existing target if
  // available. Returns true if the duplicate is found.
  bool FindDuplicateInOutgoingShareTargets(absl::string_view endpoint_id,
                                           ShareTarget& share_target);

  // Move the endpoint to the discovery cache with the given expiry time.
  void MoveToDiscoveryCache(std::string endpoint_id, uint64_t expiry_ms);

  // Move all outgoing share targets to the discovery cache so that they will be
  // reported as receive_disabled.
  void DisableAllOutgoingShareTargets();

  void CreateOutgoingShareSession(
      const ShareTarget& share_target, absl::string_view endpoint_id,
      std::optional<NearbyShareDecryptedPublicCertificate> certificate,
      absl::AnyInvocable<void(OutgoingShareSession& session,
                              const TransferMetadata& metadata)>
          transfer_update_callback);

  void ForEachShareTarget(
      absl::AnyInvocable<void(const ShareTarget&)> callback);

 private:
  // Returns the share target if it has been removed, std::nullopt otherwise.
  std::optional<ShareTarget> RemoveOutgoingShareTargetWithEndpointId(
      absl::string_view endpoint_id);

  // Cache a recently lost share target to be re-discovered.
  // Purged after expiry_timer.
  struct DiscoveryCacheEntry {
    // If needed, we can add "state" field to model "Tomb" state.
    std::unique_ptr<ThreadTimer> expiry_timer;
    ShareTarget share_target;
  };

  Clock& clock_;
  TaskRunner& service_thread_;
  NearbyConnectionsManager& connections_manager_;
  analytics::AnalyticsRecorder& analytics_recorder_;
  absl::AnyInvocable<void(const ShareTarget&)> share_target_updated_callback_;
  absl::AnyInvocable<void(const ShareTarget&)> share_target_lost_callback_;

  // A map of endpoint id to ShareTarget, where each ShareTarget entry
  // directly corresponds to a OutgoingShareSession entry in
  // outgoing_share_target_info_map_;
  absl::flat_hash_map<std::string, ShareTarget> outgoing_share_target_map_;
  // A map of ShareTarget id to OutgoingShareSession. This lets us know which
  // endpoint and public certificate are related to the outgoing share target.
  absl::flat_hash_map<int64_t, OutgoingShareSession>
      outgoing_share_session_map_;
  // A map of Endpoint id to DiscoveryCacheEntry.
  // All ShareTargets in discovery cache have received_disabled set to true.
  absl::flat_hash_map<std::string, DiscoveryCacheEntry> discovery_cache_;
};

}  // namespace nearby::sharing

#endif  // THIRD_PARTY_NEARBY_SHARING_OUTGOING_TARGETS_MANAGER_H_
