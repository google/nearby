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

#include "sharing/outgoing_targets_manager.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "internal/flags/nearby_flags.h"
#include "internal/platform/clock.h"
#include "internal/platform/task_runner.h"
#include "proto/sharing_enums.pb.h"
#include "sharing/analytics/analytics_recorder.h"
#include "sharing/certificates/nearby_share_decrypted_public_certificate.h"
#include "sharing/flags/generated/nearby_sharing_feature_flags.h"
#include "sharing/internal/public/logging.h"
#include "sharing/nearby_connections_manager.h"
#include "sharing/outgoing_share_session.h"
#include "sharing/proto/encrypted_metadata.pb.h"
#include "sharing/proto/enums.pb.h"
#include "sharing/proto/wire_format.pb.h"
#include "sharing/share_target.h"
#include "sharing/thread_timer.h"
#include "sharing/transfer_metadata.h"

namespace nearby::sharing {

OutgoingTargetsManager::OutgoingTargetsManager(
    Clock* clock, TaskRunner* service_thread,
    NearbyConnectionsManager* connections_manager,
    analytics::AnalyticsRecorder* analytics_recorder,
    absl::AnyInvocable<void(const ShareTarget&)> share_target_updated_callback,
    absl::AnyInvocable<void(const ShareTarget&)> share_target_lost_callback)
    : clock_(*clock),
      service_thread_(*service_thread),
      connections_manager_(*connections_manager),
      analytics_recorder_(*analytics_recorder),
      share_target_updated_callback_(std::move(share_target_updated_callback)),
      share_target_lost_callback_(std::move(share_target_lost_callback)) {}


void OutgoingTargetsManager::DeduplicateInOutgoingShareTarget(
    const ShareTarget& share_target, absl::string_view endpoint_id,
    std::optional<NearbyShareDecryptedPublicCertificate> certificate) {
  // TODO(b/343764269): may need to update last_outgoing_metadata_ if the
  // deduped target id matches the one in last_outgoing_metadata_.
  // But since we do not modify the share target of a connected session, it may
  // not happen.

  auto session_it = outgoing_share_session_map_.find(share_target.id);
  if (session_it == outgoing_share_session_map_.end()) {
    LOG(WARNING) << __func__ << ": share_target.id=" << share_target.id
                 << " not found in outgoing share session map.";
    return;
  }
  if (session_it->second.IsConnected()) {
    LOG(INFO) << __func__ << ": share_target.id=" << share_target.id
              << " is connected, not updating outgoing_share_session_map_.";
    return;
  }
  session_it->second.UpdateSessionForDedup(share_target, std::move(certificate),
                                           endpoint_id);

  share_target_updated_callback_(share_target);

  LOG(INFO) << __func__
            << ": [Dedupped] NotifyShareTargetUpdated to all surfaces "
               "for share_target: "
            << share_target.ToString();
}

bool OutgoingTargetsManager::FindDuplicateInDiscoveryCache(
    absl::string_view endpoint_id, ShareTarget& share_target) {
  auto it = discovery_cache_.find(endpoint_id);
  if (it != discovery_cache_.end()) {
    // If endpoint info changes for an endpoint ID, NC will send a rediscovery
    // event for the same endpoint id.
    LOG(INFO) << __func__
              << ": [Dedupped] Found duplicate endpoint_id: " << endpoint_id
              << ", share_target.id changed from: " << share_target.id << " to "
              << it->second.share_target.id;
    share_target.id = it->second.share_target.id;
    discovery_cache_.erase(it);
    return true;
  }

  for (auto it = discovery_cache_.begin(); it != discovery_cache_.end(); ++it) {
    if (it->second.share_target.device_id == share_target.device_id) {
      LOG(INFO) << __func__
                << ": [Dedupped] Found duplicate device_id, share_target.id "
                   "changed from: "
                << share_target.id << " to " << it->second.share_target.id
                << ". New endpoint_id: " << endpoint_id;
      // Share targets in discovery cache have receive_disabled set to true.
      // Copy only the id field from cache entry,
      share_target.id = it->second.share_target.id;
      discovery_cache_.erase(it);
      return true;
    }
  }
  return false;
}

bool OutgoingTargetsManager::FindDuplicateInOutgoingShareTargets(
    absl::string_view endpoint_id, ShareTarget& share_target) {
  // If the duplicate is found, share_target.id needs to be updated to the old
  // "discovered" share_target_id so NotifyShareTargetUpdated matches a target
  // that was discovered before.

  auto it = outgoing_share_target_map_.find(endpoint_id);
  if (it != outgoing_share_target_map_.end()) {
    // If endpoint info changes for an endpoint ID, NC will send a rediscovery
    // event for the same endpoint id.
    LOG(INFO) << __func__
              << ": [Dedupped] Found duplicate endpoint_id: " << endpoint_id
              << " in outgoing_share_target_map, share_target.id changed from: "
              << share_target.id << " to " << it->second.id;
    share_target.id = it->second.id;
    it->second = share_target;
    return true;
  }

  for (auto it = outgoing_share_target_map_.begin();
       it != outgoing_share_target_map_.end(); ++it) {
    if (it->second.device_id == share_target.device_id) {
      LOG(INFO)
          << __func__
          << ": [Dedupped] Found duplicate device_id, endpoint ID "
             "changed from: "
          << it->first << " to " << endpoint_id
          << " in outgoing_share_target_map, share_target.id changed from: "
          << share_target.id << " to " << it->second.id;
      share_target.id = it->second.id;
      outgoing_share_target_map_.erase(it);
      outgoing_share_target_map_.insert_or_assign(endpoint_id, share_target);
      return true;
    }
  }
  return false;
}

std::optional<ShareTarget>
OutgoingTargetsManager::RemoveOutgoingShareTargetWithEndpointId(
    absl::string_view endpoint_id) {
  VLOG(1) << __func__ << ":Outgoing connection to " << endpoint_id
          << " disconnected";
  auto target_node = outgoing_share_target_map_.extract(endpoint_id);
  if (target_node.empty()) {
    LOG(WARNING) << __func__ << ": endpoint_id=" << endpoint_id
                 << " not found in outgoing share target map.";
    return std::nullopt;
  }
  ShareTarget& share_target = target_node.mapped();
  VLOG(1) << __func__ << ": Removing (endpoint_id=" << endpoint_id
          << ", share_target.id=" << target_node.mapped().id
          << ") from outgoing share target map";

  // Do not destroy the session until it has been removed from the map.
  // Session destruction can trigger callbacks that traverses the map and it
  // cannot access the map while it is being modified.
  auto session_node =
      outgoing_share_session_map_.extract(target_node.mapped().id);
  if (!session_node.empty()) {
    session_node.mapped().OnDisconnect();
  } else {
    LOG(WARNING) << __func__ << ": share_target.id=" << target_node.mapped().id
                 << " not found in outgoing share session map.";
  }
  return share_target;
}

// Pass endpoint_id by value here since we remove entries from the
// outgoing_share_target_map_ in this function, and some callers like
// DisableAllOutgoingShareTargets pass the map item key as the endpoint_id.
// This prevents the endpoint_id from being invalidated in this function.
void OutgoingTargetsManager::MoveToDiscoveryCache(std::string endpoint_id,
                                                    uint64_t expiry_ms) {
  std::optional<ShareTarget> share_target_opt =
      RemoveOutgoingShareTargetWithEndpointId(endpoint_id);
  if (!share_target_opt.has_value()) {
    return;
  }
  DiscoveryCacheEntry cache_entry;
  cache_entry.share_target = std::move(share_target_opt.value());
  // Entries in Discovery Cache are all receive disabled.
  cache_entry.share_target.receive_disabled = true;
  cache_entry.expiry_timer = std::make_unique<ThreadTimer>(
      service_thread_, absl::StrCat("discovery_cache_timeout_", endpoint_id),
      absl::Milliseconds(expiry_ms),
      [this, expiry_ms, endpoint_id = std::string(endpoint_id)]() {
        auto cache_node = discovery_cache_.extract(endpoint_id);
        if (cache_node.empty()) {
          LOG(WARNING) << "Trying to remove endpoint_id: " << endpoint_id
                       << " from discovery_cache, but cannot find it";
          return;
        }
        ShareTarget& share_target = cache_node.mapped().share_target;
        LOG(INFO) << ": Removing (endpoint_id=" << endpoint_id
                  << ", share_target.id=" << share_target.id
                  << ") from discovery_cache after " << expiry_ms << "ms";

        share_target_lost_callback_(share_target);

        VLOG(1) << "discovery_cache entry: " << endpoint_id << " timeout after "
                << expiry_ms << "ms"
                << ": [Dedupped] NotifyShareTargetLost to all surfaces for "
                << "share_target: " << share_target.ToString();
      });
  // Send ShareTarget update to set receive disabled to true.
  share_target_updated_callback_(cache_entry.share_target);
  auto [it, inserted] =
      discovery_cache_.insert_or_assign(endpoint_id, std::move(cache_entry));
  LOG(INFO) << "[Dedupped] added to discovery_cache: " << endpoint_id << " by "
            << (inserted ? "insert" : "assign");
}

void OutgoingTargetsManager::CreateOutgoingShareSession(
    const ShareTarget& share_target, absl::string_view endpoint_id,
    std::optional<NearbyShareDecryptedPublicCertificate> certificate,
    absl::AnyInvocable<void(OutgoingShareSession& session,
                            const TransferMetadata& metadata)>
        transfer_update_callback) {
  outgoing_share_target_map_.insert_or_assign(endpoint_id, share_target);
  auto [it_out, inserted] = outgoing_share_session_map_.try_emplace(
      share_target.id, &clock_, service_thread_, &connections_manager_,
      analytics_recorder_, std::string(endpoint_id), share_target,
      std::move(transfer_update_callback));
  if (!inserted) {
    LOG(WARNING) << __func__ << ": share_target.id=" << share_target.id
                 << " already exists in outgoing share session map. This "
                    "should NOT happen";
  } else {
    auto& session = it_out->second;
    if (certificate.has_value()) {
      session.set_certificate(std::move(*certificate));
    }
  }
}

OutgoingShareSession* OutgoingTargetsManager::GetOutgoingShareSession(
    int64_t share_target_id) {
  auto it = outgoing_share_session_map_.find(share_target_id);
  if (it == outgoing_share_session_map_.end()) {
    return nullptr;
  }

  return &it->second;
}

void OutgoingTargetsManager::DisableAllOutgoingShareTargets() {
  VLOG(1) << "Move all outgoing share targets to discovery cache.";
  while (!outgoing_share_target_map_.empty()) {
    MoveToDiscoveryCache(outgoing_share_target_map_.begin()->first,
                         NearbyFlags::GetInstance().GetInt64Flag(
                             config_package_nearby::nearby_sharing_feature::
                                 kUnregisterTargetDiscoveryCacheLostExpiryMs));
  }
  DCHECK(outgoing_share_target_map_.empty());
  DCHECK(outgoing_share_session_map_.empty());
}

void OutgoingTargetsManager::Cleanup() {
  while (!outgoing_share_target_map_.empty()) {
    RemoveOutgoingShareTargetWithEndpointId(
        outgoing_share_target_map_.begin()->first);
  }
  discovery_cache_.clear();
}

void OutgoingTargetsManager::ForEachShareTarget(
    absl::AnyInvocable<void(const ShareTarget&)> callback) {
  // All share targets in discovery_cache have received_disabled set to true,
  // send them to new send surface in discovered events..
  for (const auto& [endpoint_id, discovery_cache_entry] : discovery_cache_) {
    callback(discovery_cache_entry.share_target);
  }
  for (const auto& [endpoint_id, share_target] : outgoing_share_target_map_) {
    callback(share_target);
  }
}

}  // namespace nearby::sharing
