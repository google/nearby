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

#include "core/internal/endpoint_channel_manager.h"

#include <memory>

#include "core/internal/offline_frames.h"
#include "proto/connections/offline_wire_formats.pb.h"
#include "platform/base/feature_flags.h"
#include "platform/public/logging.h"
#include "platform/public/mutex.h"
#include "platform/public/mutex_lock.h"
#include "platform/public/system_clock.h"
#include "absl/time/time.h"

namespace location {
namespace nearby {
namespace connections {

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

  NEARBY_LOG(
      INFO,
      "EndpointChannelManager registered channel of type %s to endpoint %s",
      channel->GetType().c_str(), endpoint_id.c_str());
  SetActiveEndpointChannel(client, endpoint_id, std::move(channel));
}

void EndpointChannelManager::ReplaceChannelForEndpoint(
    ClientProxy* client, const std::string& endpoint_id,
    std::unique_ptr<EndpointChannel> channel) {
  MutexLock lock(&mutex_);

  auto* endpoint = channel_state_.LookupEndpointData(endpoint_id);
  if (endpoint != nullptr && endpoint->channel == nullptr) {
    NEARBY_LOG(INFO,
               "EndpointChannelManager is missing channel while trying to "
               "update: endpoint %s",
               endpoint_id.c_str());
  }

  SetActiveEndpointChannel(client, endpoint_id, std::move(channel));
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
    NEARBY_LOG(INFO, "No channel info for endpoint %s", endpoint_id.c_str());
    return {};
  }

  return endpoint->channel;
}

void EndpointChannelManager::SetActiveEndpointChannel(
    ClientProxy* client, const std::string& endpoint_id,
    std::unique_ptr<EndpointChannel> channel) {
  // Update the channel first, then encrypt this new channel, if
  // crypto context is present.
  channel_state_.UpdateChannelForEndpoint(endpoint_id, std::move(channel));

  auto* endpoint = channel_state_.LookupEndpointData(endpoint_id);
  if (endpoint->IsEncrypted()) channel_state_.EncryptChannel(endpoint);
}

int EndpointChannelManager::GetConnectedEndpointsCount() const {
  MutexLock lock(&mutex_);
  return channel_state_.GetConnectedEndpointsCount();
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

bool EndpointChannelManager::ChannelState::RemoveEndpoint(
    const std::string& endpoint_id,
    proto::connections::DisconnectionReason reason) {
  auto item = endpoints_.find(endpoint_id);
  if (item == endpoints_.end()) return false;
  item->second.disconnect_reason = reason;
  auto channel = item->second.channel;
  if (channel) {
    // If the channel was paused (i.e. during a bandwidth upgrade negotiation)
    // we resume to ensure the thread won't hang when trying to write to it.
    channel->Resume();

    channel->Write(parser::ForDisconnection());
    NEARBY_LOG(
        INFO,
        "EndpointChannelManager reported the disconnection to endpoint %s.",
        endpoint_id.c_str());
    SystemClock::Sleep(kDataTransferDelay);
  }
  endpoints_.erase(item);
  return true;
}

bool EndpointChannelManager::UnregisterChannelForEndpoint(
    const std::string& endpoint_id) {
  MutexLock lock(&mutex_);

  if (!channel_state_.RemoveEndpoint(
          endpoint_id,
          proto::connections::DisconnectionReason::LOCAL_DISCONNECTION)) {
    return false;
  }

  NEARBY_LOG(INFO,
             "EndpointChannelManager unregistered channel for endpoint %s",
             endpoint_id.c_str());

  return true;
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
