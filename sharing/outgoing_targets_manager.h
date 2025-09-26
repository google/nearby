// Copyright 2025 Google LLC
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

#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
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

// Manages outgoing share targets and outgoing share sessions.
//
// This class is thread-compatible. All methods must be called on the service
// thread.
//
// Each discovered share target has a corresponding outgoing share session.
// When the share target is lost, the share target is moved to the
// discovery cache and the share session is destroyed.  The lost share target
// is reported as receive_disabled.  After a retention period, the share target
// is removed from the discovery cache.
// Newly discovered share targets that match share targets in the discovery
// cache are merged with the discovery cache entry and a new share session is
// created.  The share target is then removed from the discovery cache and
// reported as receive_enabled.
class OutgoingTargetsManager {
 public:
  OutgoingTargetsManager(
      Clock* absl_nonnull clock, TaskRunner* absl_nonnull service_thread,
      NearbyConnectionsManager* absl_nonnull connections_manager,
      analytics::AnalyticsRecorder* absl_nonnull analytics_recorder,
      absl::AnyInvocable<void(const ShareTarget&)>
          share_target_discovered_callback,
      absl::AnyInvocable<void(const ShareTarget&)>
          share_target_updated_callback,
      absl::AnyInvocable<void(const ShareTarget&)> share_target_lost_callback,
      std::function<void(OutgoingShareSession& session,
                         const TransferMetadata& metadata)>
          transfer_update_callback);

  // Remove all outgoing share targets and outgoing share sessions.
  // Share targets callbacks will not be called.
  // Any connected sessions will be disconnected.
  void Cleanup();

  OutgoingShareSession* GetOutgoingShareSession(int64_t share_target_id);

  void OnShareTargetDiscovered(
      ShareTarget share_target, absl::string_view endpoint_id,
      std::optional<NearbyShareDecryptedPublicCertificate> certificate);

  // Move the endpoint to the discovery cache and report the share target as
  // receive_disabled.
  // `retention` is the time to keep the share target in the discovery cache.
  void OnShareTargetLost(std::string endpoint_id, absl::Duration retention);

  // Call OnShareTargetLost() on all known share targets.
  void AllTargetsLost(absl::Duration retention);

  void ForEachShareTarget(
      absl::AnyInvocable<void(const ShareTarget&)> callback);

 private:
  // Looks for a duplicate of the share target in the outgoing share
  // target map. The share target's id is changed to match an existing target if
  // available. Returns true if the duplicate is found.
  bool FindDuplicateInOutgoingShareTargets(absl::string_view endpoint_id,
                                           ShareTarget& share_target);

  // Update the entry in outgoing_share_session_map_ with the new share target.
  // Returns true if the share target was updated.
  bool DeduplicateInOutgoingShareTarget(
      const ShareTarget& share_target, absl::string_view endpoint_id,
      std::optional<NearbyShareDecryptedPublicCertificate> certificate);

  // Looks for a duplicate of the share target in the discovery cache.
  // If found, the share target is removed from the discovery cache and its
  // id is copied into `share_target`.
  // Returns true if the duplicate is found.
  bool FindDuplicateInDiscoveryCache(absl::string_view endpoint_id,
                                     ShareTarget& share_target);

  void CreateOutgoingShareSession(
      const ShareTarget& share_target, absl::string_view endpoint_id,
      std::optional<NearbyShareDecryptedPublicCertificate> certificate);

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
  absl::AnyInvocable<void(const ShareTarget&)>
      share_target_discovered_callback_;
  absl::AnyInvocable<void(const ShareTarget&)> share_target_updated_callback_;
  absl::AnyInvocable<void(const ShareTarget&)> share_target_lost_callback_;
  std::function<void(OutgoingShareSession& session,
                     const TransferMetadata& metadata)>
      transfer_update_callback_;
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
