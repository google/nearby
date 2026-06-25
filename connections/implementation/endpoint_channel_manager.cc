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

#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/endpoint_channel.h"
#include "connections/implementation/offline_frames.h"
#include "connections/medium_selector.h"
#include "internal/platform/condition_variable.h"
#include "internal/platform/implementation/system_clock.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex.h"
#include "internal/platform/mutex_lock.h"

namespace nearby::connections {
namespace {
const absl::Duration kDataTransferDelay = absl::Milliseconds(500);
}

EndpointChannelManager::~EndpointChannelManager() {
  LOG(INFO) << "Initiating shutdown of EndpointChannelManager.";
  MutexLock lock(&mutex_);
  channel_state_.DestroyAll();
  LOG(INFO) << "EndpointChannelManager has shut down.";
}

void EndpointChannelManager::RegisterChannelForEndpoint(
    ClientProxy* client, const std::string& endpoint_id,
    std::shared_ptr<EndpointChannel> channel) {
  MutexLock lock(&mutex_);

  LOG(INFO) << "EndpointChannelManager registered channel of type "
            << channel->GetType() << " to endpoint " << endpoint_id;
  SetActiveEndpointChannel(client, endpoint_id, std::move(channel),
                           true /* enable_encryption */);

  LOG(INFO) << "Registered channel: id=" << endpoint_id;
}

void EndpointChannelManager::ReplaceChannelForEndpoint(
    ClientProxy* client, const std::string& endpoint_id,
    std::shared_ptr<EndpointChannel> channel, bool enable_encryption) {
  MutexLock lock(&mutex_);
  if (client->IsSafeToDisconnectEnabled(endpoint_id) &&
      channel_state_.IsWaitingForSafeToDisconnectTimeout(endpoint_id)) {
    LOG(WARNING)
        << "EndpointChannelManager failed to replace endpoint " << endpoint_id
        << "'s channel with type " << channel->GetType()
        << " because the endpoint is waiting for active channel closure.";
    return;
  }

  std::shared_ptr<ChannelState::EndpointData> endpoint =
      channel_state_.GetEndpointData(endpoint_id);
  if (endpoint != nullptr && endpoint->channel == nullptr) {
    LOG(INFO) << "EndpointChannelManager is missing channel while "
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
  std::shared_ptr<ChannelState::EndpointData> endpoint =
      channel_state_.GetEndpointData(endpoint_id);
  if (endpoint == nullptr) return false;
  return channel_state_.EncryptChannel(endpoint.get());
}

std::shared_ptr<EndpointChannel> EndpointChannelManager::GetChannelForEndpoint(
    absl::string_view endpoint_id) {
  MutexLock lock(&mutex_);

  std::shared_ptr<ChannelState::EndpointData> endpoint =
      channel_state_.GetEndpointData(endpoint_id);
  if (endpoint == nullptr) {
    LOG(INFO) << "No channel info for endpoint " << endpoint_id;
    return {};
  }

  return endpoint->channel;
}

void EndpointChannelManager::SetActiveEndpointChannel(
    ClientProxy* client, const std::string& endpoint_id,
    std::shared_ptr<EndpointChannel> channel, bool enable_encryption) {
  // Update the channel first, then encrypt this new channel, if
  // crypto context is present.
  channel->SetAnalyticsRecorder(&client->GetAnalyticsRecorder(), endpoint_id);
  channel->SetLocalEndpointId(client->GetLocalEndpointId());
  channel_state_.UpdateChannelForEndpoint(endpoint_id, std::move(channel));
  channel_state_.UpdateSafeToDisconnectForEndpoint(
      endpoint_id, client->IsSafeToDisconnectEnabled(endpoint_id));
  std::shared_ptr<ChannelState::EndpointData> endpoint =
      channel_state_.GetEndpointData(endpoint_id);
  if (endpoint != nullptr && endpoint->IsEncrypted() && enable_encryption)
    channel_state_.EncryptChannel(endpoint.get());
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
    const std::string& endpoint_id, bool safe_to_disconnect_enabled) {
  MutexLock lock(&mutex_);
  channel_state_.UpdateSafeToDisconnectForEndpoint(endpoint_id,
                                                   safe_to_disconnect_enabled);
}

void EndpointChannelManager::MarkEndpointStopWaitToDisconnect(
    absl::string_view endpoint_id, bool is_safe_to_disconnect,
    bool notify_stop_waiting) {
  MutexLock lock(&mutex_);
  channel_state_.MarkEndpointStopWaitToDisconnect(
      endpoint_id, is_safe_to_disconnect, notify_stop_waiting);
}

bool EndpointChannelManager::CreateNewTimeoutDisconnectedState(
    absl::string_view endpoint_id, absl::Duration timeout_millis) {
  std::shared_ptr<ChannelState::EndpointData> endpoint_data;
  {
    MutexLock lock(&mutex_);
    endpoint_data = channel_state_.GetEndpointData(endpoint_id);
  }
  if (!endpoint_data) return false;

  LOG(INFO) << "[safe-to-disconnect] "
               "Create TimeoutDisconnectedState for endpoint: "
            << endpoint_id;
  endpoint_data->CreateNewTimeoutDisconnectedState(timeout_millis);
  return true;
}

bool EndpointChannelManager::IsSafeToDisconnect(absl::string_view endpoint_id) {
  MutexLock lock(&mutex_);
  return channel_state_.IsSafeToDisconnect(endpoint_id);
}
void EndpointChannelManager::RemoveTimeoutDisconnectedState(
    absl::string_view endpoint_id) {
  MutexLock lock(&mutex_);
  channel_state_.RemoveTimeoutDisconnectedState(endpoint_id);
}

///////////////////////////////// ChannelState /////////////////////////////////

void EndpointChannelManager::ChannelState::EndpointData::
    CreateNewTimeoutDisconnectedState(absl::Duration timeout_millis) {
  MutexLock lock(&timeout_to_disconnected_mutex);
  timeout_to_disconnected_enabled = true;
  timeout_to_disconnected_notified = false;
  timeout_to_disconnected.Wait(timeout_millis);
  LOG(INFO) << "[safe-to-disconnect] Wait is done with "
            << (timeout_to_disconnected_notified ? "notification" : "timeout");
  if (!timeout_to_disconnected_notified) {
    is_safe_to_disconnect = true;
  }
  timeout_to_disconnected_notified = false;
  timeout_to_disconnected_enabled = false;
}

void EndpointChannelManager::ChannelState::EndpointData::
    MarkEndpointStopWaitToDisconnect(bool is_safe_to_disconnect,
                                     bool notify_stop_waiting) {
  MutexLock lock(&timeout_to_disconnected_mutex);
  this->is_safe_to_disconnect = is_safe_to_disconnect;
  if (!timeout_to_disconnected_enabled) return;
  if (notify_stop_waiting) {
    LOG(INFO) << "[safe-to-disconnect] Notify stop waiting before timeout.";
    timeout_to_disconnected.Notify();
    timeout_to_disconnected_notified = true;
  }
}

bool EndpointChannelManager::ChannelState::EndpointData::
    IsWaitingForSafeToDisconnectTimeout() const {
  MutexLock lock(&timeout_to_disconnected_mutex);
  return timeout_to_disconnected_enabled;
}

bool EndpointChannelManager::ChannelState::EndpointData::IsSafeToDisconnect()
    const {
  MutexLock lock(&timeout_to_disconnected_mutex);
  return is_safe_to_disconnect;
}

void EndpointChannelManager::ChannelState::EndpointData::
    RemoveTimeoutDisconnectedState() {
  MutexLock lock(&timeout_to_disconnected_mutex);
  timeout_to_disconnected_notified = false;
  timeout_to_disconnected_enabled = false;
}

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

std::shared_ptr<EndpointChannelManager::ChannelState::EndpointData>
EndpointChannelManager::ChannelState::GetEndpointData(
    absl::string_view endpoint_id) {
  auto it = endpoints_.find(endpoint_id);
  return it != endpoints_.end() ? it->second : nullptr;
}

void EndpointChannelManager::ChannelState::DestroyAll() {
  for (auto& item : endpoints_) {
    RemoveEndpoint(item.first, DisconnectionReason::SHUTDOWN,
                   /* safe_to_disconnect_enabled */ false,
                   SafeDisconnectionResult::kSafeDisconnection);
  }
  endpoints_.clear();
}

void EndpointChannelManager::ChannelState::UpdateChannelForEndpoint(
    const std::string& endpoint_id, std::shared_ptr<EndpointChannel> channel) {
  // Create EndpointData instance, if necessary, and populate channel.
  std::shared_ptr<EndpointData>& endpoint_data = endpoints_[endpoint_id];
  if (endpoint_data == nullptr) {
    endpoint_data = std::make_shared<EndpointData>();
  }
  endpoint_data->channel = std::move(channel);
}

void EndpointChannelManager::ChannelState::UpdateEncryptionContextForEndpoint(
    const std::string& endpoint_id,
    std::unique_ptr<EncryptionContext> context) {
  // Create EndpointData instance, if necessary, and populate crypto context.
  std::shared_ptr<EndpointData>& endpoint_data = endpoints_[endpoint_id];
  if (endpoint_data == nullptr) {
    endpoint_data = std::make_shared<EndpointData>();
  }
  endpoint_data->context = std::move(context);
}

void EndpointChannelManager::ChannelState::UpdateSafeToDisconnectForEndpoint(
    const std::string& endpoint_id, bool safe_to_disconnect_enabled) {
  LOG(INFO) << "[safe-to-disconnect] "
               "UpdateSafeToDisconnectForEndpoint for: "
            << endpoint_id << " " << safe_to_disconnect_enabled;

  std::shared_ptr<EndpointData>& endpoint_data = endpoints_[endpoint_id];
  if (endpoint_data == nullptr) {
    endpoint_data = std::make_shared<EndpointData>();
  }
  endpoint_data->safe_to_disconnect_enabled = safe_to_disconnect_enabled;
}

bool EndpointChannelManager::ChannelState::GetSafeToDisconnectForEndpoint(
    absl::string_view endpoint_id) {
  auto it = endpoints_.find(endpoint_id);
  if (it == endpoints_.end()) return false;
  LOG(INFO) << "[safe-to-disconnect] GetSafeToDisconnectForEndpoint: "
            << it->second->safe_to_disconnect_enabled;
  return it->second->safe_to_disconnect_enabled;
}

bool EndpointChannelManager::ChannelState::RemoveEndpoint(
    absl::string_view endpoint_id, DisconnectionReason reason,
    bool safe_to_disconnect_enabled, SafeDisconnectionResult result) {
  auto it = endpoints_.find(endpoint_id);
  if (it == endpoints_.end()) return false;

  MarkEndpointStopWaitToDisconnect(endpoint_id,
                                   /* is_safe_to_disconnect */ true,
                                   /* notify_stop_waiting */ true);
  it->second->disconnect_reason = reason;
  std::shared_ptr<EndpointChannel> channel = it->second->channel;

  if (channel && !channel->IsClosed() && !safe_to_disconnect_enabled) {
    // If the channel was paused (i.e. during a bandwidth upgrade negotiation)
    // we resume to ensure the thread won't hang when trying to write to it.
    channel->Resume();

    LOG(INFO) << "[safe-to-disconnect] Sending DISCONNECTION frame"
                 " with request 0, ack 0";
    channel->Write(
        parser::ForDisconnection(/* request_safe_to_disconnect */ false,
                                 /* ack_safe_to_disconnect */ false));
    LOG(INFO)
        << "EndpointChannelManager reported the disconnection to endpoint "
        << endpoint_id;
    SystemClock::Sleep(kDataTransferDelay);
  }

  LOG(INFO) << "Remove Endpoint: " << endpoint_id;
  endpoints_.erase(it);
  return true;
}

bool EndpointChannelManager::ChannelState::isWifiLanConnected() const {
  for (auto& endpoint : endpoints_) {
    std::shared_ptr<EndpointChannel> channel = endpoint.second->channel;
    if (channel) {
      if (channel->GetMedium() == Medium::WIFI_LAN) {
        LOG(INFO) << "Found WIFI_LAN Medium for endpoint:" << endpoint.first;
        return true;
      }
    }
  }

  return false;
}

void EndpointChannelManager::ChannelState::MarkEndpointStopWaitToDisconnect(
    absl::string_view endpoint_id, bool is_safe_to_disconnect,
    bool notify_stop_waiting) {
  auto it = endpoints_.find(endpoint_id);
  if (it == endpoints_.end()) return;
  LOG(INFO) << "[safe-to-disconnect] is_safe_to_disconnect= "
            << is_safe_to_disconnect
            << ", notify_stop_waiting= " << notify_stop_waiting
            << " for endpoint: " << endpoint_id;
  it->second->MarkEndpointStopWaitToDisconnect(is_safe_to_disconnect,
                                               notify_stop_waiting);
}

bool EndpointChannelManager::ChannelState::IsWaitingForSafeToDisconnectTimeout(
    absl::string_view endpoint_id) {
  auto it = endpoints_.find(endpoint_id);
  if (it == endpoints_.end()) return false;
  bool enabled = it->second->IsWaitingForSafeToDisconnectTimeout();
  LOG(INFO) << "[safe-to-disconnect] "
               "IsWaitingForSafeToDisconnectTimeout for endpoint: "
            << endpoint_id << ": " << enabled;
  return enabled;
}

bool EndpointChannelManager::ChannelState::IsSafeToDisconnect(
    absl::string_view endpoint_id) {
  auto it = endpoints_.find(endpoint_id);
  if (it == endpoints_.end()) return true;
  bool is_safe = it->second->IsSafeToDisconnect();
  LOG(INFO) << "[safe-to-disconnect] Get SafeToDisconnect status for endpoint: "
            << endpoint_id << ": " << is_safe;
  return is_safe;
}

void EndpointChannelManager::ChannelState::RemoveTimeoutDisconnectedState(
    absl::string_view endpoint_id) {
  auto it = endpoints_.find(endpoint_id);
  if (it == endpoints_.end()) return;
  it->second->RemoveTimeoutDisconnectedState();
}

bool EndpointChannelManager::UnregisterChannelForEndpoint(
    absl::string_view endpoint_id, DisconnectionReason reason,
    SafeDisconnectionResult result) {
  MutexLock lock(&mutex_);

  bool safe_to_disconnect_enabled =
      channel_state_.GetSafeToDisconnectForEndpoint(endpoint_id);
  if (!channel_state_.RemoveEndpoint(endpoint_id, reason,
                                     safe_to_disconnect_enabled, result)) {
    return false;
  }
  LOG(INFO) << "EndpointChannelManager unregistered channel for endpoint "
            << endpoint_id;
  return true;
}

}  // namespace nearby::connections
