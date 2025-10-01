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

#include "sharing/outgoing_targets_manager.h"

#include <stdint.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/functional/any_invocable.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "internal/platform/clock.h"
#include "internal/platform/task_runner.h"
#include "proto/sharing_enums.pb.h"
#include "sharing/analytics/analytics_recorder.h"
#include "sharing/certificates/nearby_share_decrypted_public_certificate.h"
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
    Clock* absl_nonnull clock, TaskRunner* absl_nonnull service_thread,
    NearbyConnectionsManager* absl_nonnull connections_manager,
    analytics::AnalyticsRecorder* analytics_recorder,
    absl::AnyInvocable<void(const ShareTarget&)>
        share_target_discovered_callback,
    absl::AnyInvocable<void(const ShareTarget&)> share_target_updated_callback,
    absl::AnyInvocable<void(const ShareTarget&)> share_target_lost_callback,
    std::function<void(OutgoingShareSession& session,
                       const TransferMetadata& metadata)>
        transfer_update_callback)
    : clock_(*clock),
      service_thread_(*service_thread),
      connections_manager_(*connections_manager),
      analytics_recorder_(*analytics_recorder),
      share_target_discovered_callback_(
          std::move(share_target_discovered_callback)),
      share_target_updated_callback_(std::move(share_target_updated_callback)),
      share_target_lost_callback_(std::move(share_target_lost_callback)),
      transfer_update_callback_(std::move(transfer_update_callback)) {}

void OutgoingTargetsManager::OnShareTargetDiscovered(
    ShareTarget share_target, absl::string_view endpoint_id,
    std::optional<NearbyShareDecryptedPublicCertificate> certificate) {
  std::optional<int64_t> old_id =
      FindExistingTargetId(endpoint_id, share_target);
  if (old_id.has_value()) {
    // If the duplicate is found, share_target.id needs to be updated to the old
    // "discovered" share_target_id so ShareTarget updates matches a target
    // that was discovered before.
    share_target.id = *old_id;
    LOG(INFO) << __func__
              << ": ShareTarget updated, endpoint_id: " << endpoint_id
              << ", share_target: " << share_target.ToString();
    auto session_it = outgoing_share_session_map_.find(*old_id);
    if (session_it != outgoing_share_session_map_.end()) {
      OutgoingShareSession& session = session_it->second;
      std::string old_endpoint_id = session.endpoint_id();
      if (session.UpdateSessionForDedup(share_target, std::move(certificate),
                                        endpoint_id) &&
          old_endpoint_id != endpoint_id) {
        // Session updated and endpoint_id changed.  Need to update endpoint id
        // to share target id mapping.
        outgoing_target_id_map_.erase(old_endpoint_id);
        outgoing_target_id_map_.insert(
            {std::string(endpoint_id), share_target.id});
      }
    }
    share_target_updated_callback_(share_target);
    return;
  }
  old_id = FindInDiscoveryCache(endpoint_id, share_target);
  bool in_discovery_cache = old_id.has_value();
  if (in_discovery_cache) {
    share_target.id = *old_id;
  }
  LOG(INFO) << __func__
            << (in_discovery_cache ? ": Recovered from discovery cache"
                                   : ": Discovered new target")
            << ": endpoint_id=" << endpoint_id
            << ", share_target=" << share_target.ToString();
  AddTarget(share_target, endpoint_id, std::move(certificate));
  if (in_discovery_cache) {
    share_target_updated_callback_(share_target);
  } else {
    share_target_discovered_callback_(share_target);
  }
}

std::optional<int64_t> OutgoingTargetsManager::FindInDiscoveryCache(
    absl::string_view endpoint_id, const ShareTarget& share_target) {
  auto it = discovery_cache_.find(endpoint_id);
  if (it != discovery_cache_.end()) {
    int64_t old_id = it->second.share_target.id;
    // If endpoint info changes for an endpoint ID, NC will send a rediscovery
    // event for the same endpoint id.
    LOG(INFO) << __func__ << ": Found existing endpoint_id: " << endpoint_id
              << ", mapping share_target.id: " << share_target.id
              << " to: " << it->second.share_target.id;
    discovery_cache_.erase(it);
    return old_id;
  }

  if (share_target.device_id.empty()) {
    // Do not match empty device_id.
    return std::nullopt;
  }
  auto device_id_it = std::find_if(
      discovery_cache_.begin(), discovery_cache_.end(),
      [&share_target](const auto& pair) {
        return pair.second.share_target.device_id == share_target.device_id;
      });
  if (device_id_it != discovery_cache_.end()) {
    int64_t old_id = device_id_it->second.share_target.id;
    LOG(INFO) << __func__
              << ": Found existing device_id, updating endpoint ID: "
              << device_id_it->first << " to: " << endpoint_id
              << " , mapping share_target.id: " << share_target.id
              << " to: " << old_id;
    discovery_cache_.erase(device_id_it);
    return old_id;
  }
  return std::nullopt;
}

std::optional<int64_t> OutgoingTargetsManager::FindExistingTargetId(
    absl::string_view endpoint_id,
    const ShareTarget& share_target) {
  auto it = outgoing_target_id_map_.find(endpoint_id);
  if (it != outgoing_target_id_map_.end()) {
    int64_t old_share_target_id = it->second;
    // If endpoint info changes for an endpoint ID, NC will send a rediscovery
    // event for the same endpoint id.
    LOG(INFO) << __func__ << ": Found existing endpoint_id: " << endpoint_id
              << ", mapping share_target.id: " << share_target.id
              << " to: " << old_share_target_id;
    return old_share_target_id;
  }

  if (share_target.device_id.empty()) {
    // Do not match empty device_id.
    return std::nullopt;
  }
  auto device_id_it = std::find_if(
      outgoing_share_session_map_.begin(), outgoing_share_session_map_.end(),
      [&share_target](const auto& pair) {
        return pair.second.share_target().device_id == share_target.device_id;
      });
  if (device_id_it != outgoing_share_session_map_.end()) {
    int64_t old_share_target_id = device_id_it->second.share_target().id;
    LOG(INFO) << __func__
              << ": Found existing device_id, updating endpoint ID: "
              << device_id_it->first << " to: " << endpoint_id
              << " , mapping share_target.id: " << share_target.id
              << " to: " << old_share_target_id;
    return old_share_target_id;
  }
  return std::nullopt;
}

std::optional<ShareTarget> OutgoingTargetsManager::RemoveTarget(
    absl::string_view endpoint_id, bool close_connected) {
  VLOG(1) << __func__ << ":Removing endpoint_id " << endpoint_id;
  auto it = outgoing_target_id_map_.find(endpoint_id);
  if (it == outgoing_target_id_map_.end()) {
    LOG(WARNING) << __func__ << ": endpoint_id=" << endpoint_id
                 << " not found.";
    return std::nullopt;
  }
  int64_t share_target_id = it->second;
  VLOG(1) << __func__
          << ": Removing share_target.id=" << share_target_id
          << " from outgoing share target map";

  auto session_it = outgoing_share_session_map_.find(share_target_id);
  if (session_it == outgoing_share_session_map_.end()) {
    LOG(ERROR) << __func__ << ": share_target.id=" << share_target_id
                 << " not found in outgoing share session map.";
    return std::nullopt;
  }
  if (!close_connected && session_it->second.IsConnected()) {
    LOG(INFO) << __func__ << ": share_target.id=" << share_target_id
              << " is connected, not removing.";
    return std::nullopt;
  }
  outgoing_target_id_map_.erase(it);
  // Do not destroy the session until it has been removed from the map.
  // Session destruction can trigger callbacks that traverses the map and it
  // cannot access the map while it is being modified.
  auto session_node = outgoing_share_session_map_.extract(session_it);
  ShareTarget removed_target = session_node.mapped().share_target();
  session_node.mapped().OnDisconnect();
  return std::move(removed_target);
}

// Pass endpoint_id by value here since we remove entries from the
// outgoing_target_id_map_ in this function, and some callers like
// AllTargetsLost pass the map item key as the endpoint_id.
// This prevents the endpoint_id from being invalidated in this function.
void OutgoingTargetsManager::OnShareTargetLost(std::string endpoint_id,
                                               absl::Duration retention) {
  std::optional<ShareTarget> share_target_opt =
      RemoveTarget(endpoint_id, /*close_connected=*/false);
  if (!share_target_opt.has_value()) {
    return;
  }
  DiscoveryCacheEntry cache_entry;
  cache_entry.share_target = std::move(share_target_opt.value());
  // Entries in Discovery Cache are all receive disabled.
  cache_entry.share_target.receive_disabled = true;
  cache_entry.expiry_timer = std::make_unique<ThreadTimer>(
      service_thread_, absl::StrCat("discovery_cache_timeout_", endpoint_id),
      retention, [this, retention, endpoint_id]() {
        auto cache_node = discovery_cache_.extract(endpoint_id);
        if (cache_node.empty()) {
          LOG(WARNING) << "Trying to remove endpoint_id: " << endpoint_id
                       << " from discovery_cache, but cannot find it";
          return;
        }
        ShareTarget& share_target = cache_node.mapped().share_target;
        LOG(INFO) << ": ShareTarget lost after retention: " << retention
                  << ", endpoint_id=" << endpoint_id
                  << ", share_target=" << share_target.ToString();
        share_target_lost_callback_(share_target);
      });
  // Send ShareTarget update to set receive disabled to true.
  LOG(INFO) << __func__
            << ": ShareTarget disabled, endpoint_id: " << endpoint_id << ", "
            << cache_entry.share_target.ToString();
  share_target_updated_callback_(cache_entry.share_target);
  discovery_cache_.insert_or_assign(endpoint_id, std::move(cache_entry));
}

void OutgoingTargetsManager::AddTarget(
    const ShareTarget& share_target, absl::string_view endpoint_id,
    std::optional<NearbyShareDecryptedPublicCertificate> certificate) {
  auto [target_it, target_inserted] = outgoing_target_id_map_.insert(
      {std::string(endpoint_id), share_target.id});
  if (!target_inserted) {
    if (target_it->second != share_target.id) {
      LOG(ERROR) << __func__ << ": endpoint_id=" << endpoint_id
                 << " already associated with share_target id="
                 << target_it->second
                 << ", cannot replace with share_target id=" << share_target.id;
      return;
    }
    LOG(WARNING) << __func__ << ": endpoint_id=" << endpoint_id
                 << " already exists, share target not updated.";
  }
  auto [session_it, session_inserted] = outgoing_share_session_map_.try_emplace(
      share_target.id, &clock_, service_thread_, &connections_manager_,
      analytics_recorder_, std::string(endpoint_id), share_target,
      transfer_update_callback_);
  if (!session_inserted) {
    LOG(WARNING) << __func__ << ": share_target.id=" << share_target.id
                 << " already exists in outgoing share session map. This "
                    "should NOT happen";
    if (session_it->second.IsConnected()) {
      LOG(WARNING) << __func__
                   << ": session for share_target.id=" << share_target.id
                   << " is connected, certificate not updated.";
      return;
    }
  }
  auto& session = session_it->second;
  if (certificate.has_value()) {
    session.set_certificate(std::move(*certificate));
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

void OutgoingTargetsManager::AllTargetsLost(absl::Duration retention) {
  VLOG(1) << "Move all outgoing share targets to discovery cache.";
  while (!outgoing_target_id_map_.empty()) {
    OnShareTargetLost(outgoing_target_id_map_.begin()->first, retention);
  }
}

void OutgoingTargetsManager::Cleanup() {
  while (!outgoing_target_id_map_.empty()) {
    // Latch endpoint_id here since RemoveTarget() will remove the entry from
    // the map.
    std::string endpoint_id = outgoing_target_id_map_.begin()->first;
    RemoveTarget(endpoint_id, /*close_connected=*/true);
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
  for (const auto& [target_id, session] : outgoing_share_session_map_) {
    callback(session.share_target());
  }
}

}  // namespace nearby::sharing
