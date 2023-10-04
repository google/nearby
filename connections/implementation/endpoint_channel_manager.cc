// Copyright 2020 Google LLC
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

#include "connections/implementation/endpoint_channel_manager.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/time/time.h"
#include "connections/implementation/offline_frames.h"
#include "internal/platform/condition_variable.h"
#include "internal/platform/feature_flags.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex.h"
#include "internal/platform/mutex_lock.h"
#include "proto/connections_enums.pb.h"

namespace nearby {
namespace connections {
using ::location::nearby::analytics::proto::ConnectionsLog;

namespace {
const absl::Duration kDataTransferDelay = absl::Milliseconds(500);
}

EndpointChannelManager::~EndpointChannelManager() {
  NEARBY_LOG(INFO, "Initiating shutdown of EndpointChannelManager.");
  MutexLock lock(&mutex_);
  channel_state_.DestroyAll();
  NEARBY_LOG(INFO, "EndpointChannelManager has shut down.");
}

void EndpointChannelManager::RegisterChannelForEndpoint(
    ClientProxy* client, const std::string& endpoint_id,
    std::unique_ptr<EndpointChannel> channel) {
  MutexLock lock(&mutex_);

  NEARBY_LOGS(INFO) << "EndpointChannelManager registered channel of type "
                    << channel->GetType() << " to endpoint " << endpoint_id;
  SetActiveEndpointChannel(client, endpoint_id, std::move(channel),
                           true /* enable_encryption */);

  NEARBY_LOG(INFO, "Registered channel: id=%s", endpoint_id.c_str());
}

void EndpointChannelManager::ReplaceChannelForEndpoint(
    ClientProxy* client, const std::string& endpoint_id,
    std::unique_ptr<EndpointChannel> channel, bool enable_encryption) {
  MutexLock lock(&mutex_);

  auto* endpoint = channel_state_.LookupEndpointData(endpoint_id);
  if (endpoint != nullptr && endpoint->channel == nullptr) {
    NEARBY_LOGS(INFO) << "EndpointChannelManager is missing channel while "
                         "trying to update: endpoint "
                      << endpoint_id;
  }
  SetActiveEndpointChannel(client, endpoint_id, std::move(channel),
                           enable_encryption);
}

bool EndpointChannelManager::EncryptChannelForEndpoint(
    const std::string& endpoint_id,
    std::unique_ptr<EncryptionContext> context) {
  MutexLock lock(&mutex_);

  channel_state_.UpdateEncryptionContextForEndpoint(endpoint_id,
                                                    std::move(context));
  auto* endpoint = channel_state_.LookupEndpointData(endpoint_id);
  return channel_state_.EncryptChannel(endpoint);
}

std::shared_ptr<EndpointChannel> EndpointChannelManager::GetChannelForEndpoint(
    const std::string& endpoint_id) {
  MutexLock lock(&mutex_);

  auto* endpoint = channel_state_.LookupEndpointData(endpoint_id);
  if (endpoint == nullptr) {
    NEARBY_LOGS(INFO) << "No channel info for endpoint " << endpoint_id;
    return {};
  }

  return endpoint->channel;
}

void EndpointChannelManager::SetActiveEndpointChannel(
    ClientProxy* client, const std::string& endpoint_id,
    std::unique_ptr<EndpointChannel> channel, bool enable_encryption) {
  // Update the channel first, then encrypt this new channel, if
  // crypto context is present.
  channel->SetAnalyticsRecorder(&client->GetAnalyticsRecorder(), endpoint_id);
  channel_state_.UpdateChannelForEndpoint(endpoint_id, std::move(channel));
  channel_state_.UpdateSafeToDisconnectForEndpoint(
      endpoint_id, client->IsSafeToDisconnectEnabled(endpoint_id));
  auto* endpoint = channel_state_.LookupEndpointData(endpoint_id);
  if (endpoint->IsEncrypted() && enable_encryption)
    channel_state_.EncryptChannel(endpoint);
}

int EndpointChannelManager::GetConnectedEndpointsCount() const {
  MutexLock lock(&mutex_);
  return channel_state_.GetConnectedEndpointsCount();
}

bool EndpointChannelManager::isWifiLanConnected() const {
  MutexLock lock(&mutex_);
  return channel_state_.isWifiLanConnected();
}

void EndpointChannelManager::UpdateSafeToDisconnectForEndpoint(
    const std::string& endpoint_id,
    bool safe_to_disconnect_enabled) {
  MutexLock lock(&mutex_);
  channel_state_.UpdateSafeToDisconnectForEndpoint(endpoint_id,
                                                  safe_to_disconnect_enabled);
}

void EndpointChannelManager::MarkEndpointStopWaitToDisconnect(
    const std::string& endpoint_id, bool is_safe_to_disconnect,
    bool notify_stop_waiting) {
  MutexLock lock(&mutex_);
  channel_state_.MarkEndpointStopWaitToDisconnect(
      endpoint_id, is_safe_to_disconnect, notify_stop_waiting);
}

bool EndpointChannelManager::CreateNewTimeoutDisconnectedState(
    const std::string& endpoint_id, absl::Duration timeout_millis) {
  return channel_state_.CreateNewTimeoutDisconnectedState(endpoint_id,
                                                          timeout_millis);
}

bool EndpointChannelManager::IsSafeToDisconnect(
    const std::string& endpoint_id) {
  return channel_state_.IsSafeToDisconnect(endpoint_id);
}
void EndpointChannelManager::RemoveTimeoutDisconnectedState(
    const std::string& endpoint_id) {
  MutexLock lock(&mutex_);
  channel_state_.RemoveTimeoutDisconnectedState(endpoint_id);
}

///////////////////////////////// ChannelState /////////////////////////////////

// endpoint - channel endpoint to encrypt
bool EndpointChannelManager::ChannelState::EncryptChannel(
    EndpointChannelManager::ChannelState::EndpointData* endpoint) {
  if (endpoint != nullptr && endpoint->channel != nullptr &&
      endpoint->context != nullptr) {
    endpoint->channel->EnableEncryption(endpoint->context);
    return true;
  }
  return false;
}

EndpointChannelManager::ChannelState::EndpointData*
EndpointChannelManager::ChannelState::LookupEndpointData(
    const std::string& endpoint_id) {
  auto item = endpoints_.find(endpoint_id);
  return item != endpoints_.end() ? &item->second : nullptr;
}

void EndpointChannelManager::ChannelState::DestroyAll() {
  for (auto& item : endpoints_) {
    RemoveEndpoint(item.first, DisconnectionReason::SHUTDOWN,
                   /* safe_to_disconnect_enabled */ false,
                   ConnectionsLog::EstablishedConnection::SAFE_DISCONNECTION);
  }
  endpoints_.clear();
}

void EndpointChannelManager::ChannelState::UpdateChannelForEndpoint(
    const std::string& endpoint_id, std::unique_ptr<EndpointChannel> channel) {
  // Create EndpointData instance, if necessary, and populate channel.
  endpoints_[endpoint_id].channel = std::move(channel);
}

void EndpointChannelManager::ChannelState::UpdateEncryptionContextForEndpoint(
    const std::string& endpoint_id,
    std::unique_ptr<EncryptionContext> context) {
  // Create EndpointData instance, if necessary, and populate crypto context.
  endpoints_[endpoint_id].context = std::move(context);
}

void EndpointChannelManager::ChannelState::UpdateSafeToDisconnectForEndpoint(
    const std::string& endpoint_id,
    bool safe_to_disconnect_enabled) {
  NEARBY_LOGS(INFO) << "[safe-to-disconnect] "
                       "UpdateSafeToDisconnectForEndpoint for: "
                    << endpoint_id << " " << safe_to_disconnect_enabled;

  endpoints_[endpoint_id].safe_to_disconnect_enabled =
      safe_to_disconnect_enabled;
}

bool EndpointChannelManager::ChannelState::GetSafeToDisconnectForEndpoint(
    const std::string& endpoint_id) {
  auto item = endpoints_.find(endpoint_id);
  if (item == endpoints_.end()) return false;
  NEARBY_LOGS(INFO) << "[safe-to-disconnect] GetSafeToDisconnectForEndpoint: "
                    << item->second.safe_to_disconnect_enabled;
  return item->second.safe_to_disconnect_enabled;
}

bool EndpointChannelManager::ChannelState::RemoveEndpoint(
    const std::string& endpoint_id, DisconnectionReason reason,
    bool safe_to_disconnect_enabled, SafeDisconnectionResult result) {
  auto item = endpoints_.find(endpoint_id);
  if (item == endpoints_.end()) return false;

  MarkEndpointStopWaitToDisconnect(endpoint_id,
                                   /* is_safe_to_disconnect */ true,
                                   /* notify_stop_waiting */ true);
  item->second.disconnect_reason = reason;
  auto channel = item->second.channel;

  if (channel && !safe_to_disconnect_enabled) {
    // If the channel was paused (i.e. during a bandwidth upgrade negotiation)
    // we resume to ensure the thread won't hang when trying to write to it.
    channel->Resume();

    NEARBY_LOGS(INFO) << "[safe-to-disconnect] Sending DISCONNECTION frame"
                         " with request 0, ack 0";
    channel->Write(
        parser::ForDisconnection(/* request_safe_to_disconnect */ false,
                                 /* ack_safe_to_disconnect */ false));
    NEARBY_LOGS(INFO)
        << "EndpointChannelManager reported the disconnection to endpoint "
        << endpoint_id;
    SystemClock::Sleep(kDataTransferDelay);
  }
  NEARBY_LOGS(INFO) << "Remove Endpoint: " << endpoint_id;
  endpoints_.erase(item);
  return true;
}

bool EndpointChannelManager::ChannelState::isWifiLanConnected() const {
  for (auto& endpoint : endpoints_) {
    auto channel = endpoint.second.channel;
    if (channel) {
      if (channel->GetMedium() == Medium::WIFI_LAN) {
        NEARBY_LOGS(INFO) << "Found WIFI_LAN Medium for endpoint:"
                          << endpoint.first;
        return true;
      }
    }
  }

  return false;
}

void EndpointChannelManager::ChannelState::MarkEndpointStopWaitToDisconnect(
    const std::string& endpoint_id, bool is_safe_to_disconnect,
    bool notify_stop_waiting) {
  auto item = endpoints_.find(endpoint_id);
  if (item == endpoints_.end()) return;
  NEARBY_LOGS(INFO) << "[safe-to-disconnect] is_safe_to_disconnect= "
                    << is_safe_to_disconnect
                    << ", notify_stop_waiting= " << notify_stop_waiting
                    << " for endpoint: " << endpoint_id;
  {
    MutexLock lock(&item->second.timeout_to_disconnected_mutex);
    item->second.is_safe_to_disconnect = is_safe_to_disconnect;
    if (!item->second.timeout_to_disconnected_enabled) return;
    if (notify_stop_waiting) {
      NEARBY_LOGS(INFO) << "[safe-to-disconnect] Notify stop "
                           "waiting before timeout.";
      item->second.timeout_to_disconnected.Notify();
      item->second.timeout_to_disconnected_notified = true;
    }
  }
}

bool EndpointChannelManager::ChannelState::CreateNewTimeoutDisconnectedState(
    const std::string& endpoint_id, absl::Duration timeout_millis) {
  auto item = endpoints_.find(endpoint_id);
  if (item == endpoints_.end()) return false;
  NEARBY_LOGS(INFO) << "[safe-to-disconnect] "
                       "Create TimeoutDisconnectedState for endpoint: "
                    << endpoint_id;
  {
    MutexLock lock(&item->second.timeout_to_disconnected_mutex);
    item->second.timeout_to_disconnected_enabled = true;
    item->second.timeout_to_disconnected_notified = false;
    item->second.timeout_to_disconnected.Wait(timeout_millis);
    NEARBY_LOGS(INFO) << "[safe-to-disconnect] Wait is done with "
                      << (item->second.timeout_to_disconnected_notified
                              ? "notification"
                              : "timeout");
    if (!item->second.timeout_to_disconnected_notified)
      item->second.is_safe_to_disconnect = true;
    item->second.timeout_to_disconnected_notified = false;
    item->second.timeout_to_disconnected_enabled = false;
  }
  return true;
}

bool EndpointChannelManager::ChannelState::IsSafeToDisconnect(
    const std::string& endpoint_id) {

  auto item = endpoints_.find(endpoint_id);
  if (item == endpoints_.end()) return true;
  {
    MutexLock lock(&item->second.timeout_to_disconnected_mutex);
    NEARBY_LOGS(INFO)
        << "[safe-to-disconnect] Get SafeToDisconnect status for endpoint: "
        << endpoint_id << ": " << item->second.is_safe_to_disconnect;
    return (item->second.is_safe_to_disconnect);
  }
}

void EndpointChannelManager::ChannelState::RemoveTimeoutDisconnectedState(
    const std::string& endpoint_id) {
  auto item = endpoints_.find(endpoint_id);
  if (item == endpoints_.end()) return;
  {
    MutexLock lock(&item->second.timeout_to_disconnected_mutex);
    item->second.timeout_to_disconnected_notified = false;
    item->second.timeout_to_disconnected_enabled = false;
  }
}

bool EndpointChannelManager::UnregisterChannelForEndpoint(
    const std::string& endpoint_id, DisconnectionReason reason,
    SafeDisconnectionResult result) {
  MutexLock lock(&mutex_);

  auto safe_to_disconnect_enabled =
      channel_state_.GetSafeToDisconnectForEndpoint(endpoint_id);
  if (!channel_state_.RemoveEndpoint(endpoint_id, reason,
                                     safe_to_disconnect_enabled, result)) {
    return false;
  }
  NEARBY_LOGS(INFO)
      << "EndpointChannelManager unregistered channel for endpoint "
      << endpoint_id;
  return true;
}

}  // namespace connections
}  // namespace nearby
