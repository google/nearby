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

#ifndef CORE_INTERNAL_ENDPOINT_CHANNEL_MANAGER_H_
#define CORE_INTERNAL_ENDPOINT_CHANNEL_MANAGER_H_

#include <memory>
#include <string>

#include "securegcm/d2d_connection_context_v1.h"
#include "absl/container/flat_hash_map.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/endpoint_channel.h"
#include "internal/platform/mutex.h"

namespace nearby {
namespace connections {

// NOTE(std::string):
// All the strings in internal class public interfaces should be exchanged as
// const std::string& if they are immutable, and as std::string if they are
// mutable.
// This is to keep all the internal classes compatible with each other,
// and minimize resources spent on the type conversion.
// Project-wide, strings are either passed around as reference (which has
// zero maintenance costs, and sizeof(void*) memory usage => passed around in a
// CPU register), and whenever lifetime etension is required, it must be copied
// to std::string instance (which will again propagate as a const reference
// within it's lifetime domain).

// Manages the communication channels to all the remote endpoints with which we
// are interacting.
class EndpointChannelManager final {
 public:
  using EncryptionContext = EndpointChannel::EncryptionContext;

  ~EndpointChannelManager();

  // Registers the initial EndpointChannel to be associated with an endpoint;
  // if there already exists a previously-associated EndpointChannel, that will
  // be closed before continuing the registration.
  void RegisterChannelForEndpoint(ClientProxy* client,
                                  const std::string& endpoint_id,
                                  std::unique_ptr<EndpointChannel> channel)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Replaces the EndpointChannel to be associated with an endpoint from here on
  // in, transferring the encryption context from the previous EndpointChannel
  // to the newly-provided EndpointChannel.
  void ReplaceChannelForEndpoint(ClientProxy* client,
                                 const std::string& endpoint_id,
                                 std::unique_ptr<EndpointChannel> channel,
                                 bool enable_encryption)
      ABSL_LOCKS_EXCLUDED(mutex_);

  bool EncryptChannelForEndpoint(const std::string& endpoint_id,
                                 std::unique_ptr<EncryptionContext> context)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // NOTE(shared_ptr<> usage):
  //
  // EndpointChannelManager is holding an EndpointChannel instance;
  // GetChannelForEndpoint() is passing ownership over to a worker thread.
  // It is not a pointer passing but an ownership passing, to guarantee that
  // channel instance will not disappear underneath the feet of a worker thread
  // inside EndpointManager [ EndpointManager::EndpointChannelLoopRunnable() ].
  // If it is just a pointer, Channel will get destroyed while in use by a
  // worker thread. shared_ptr is a simple and reliable tool to avoid that.
  //
  // The reason why it can not be std::unique_ptr<> is: there are other code
  // paths that expect to be able to read the pointer value multiple times, from
  // multiple places (each of them needs "ownership" for the duration of their
  // use). EndpointManager::SendTransferFrameBytes() is another such place.
  // If EndpointChannelManager replaces the current channel, and any (or both)
  // EndpointManager methods that use a channel are running, it is better to
  // have a shared ownership.
  std::shared_ptr<EndpointChannel> GetChannelForEndpoint(
      const std::string& endpoint_id) ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns true if 'endpoint_id' actually had a registered EndpointChannel.
  // IOW, a return of false signifies a no-op.
  bool UnregisterChannelForEndpoint(const std::string& endpoint_id)
      ABSL_LOCKS_EXCLUDED(mutex_);

  int GetConnectedEndpointsCount() const ABSL_LOCKS_EXCLUDED(mutex_);

  // Check if any endpoint uses WLAN Medium
  bool isWifiLanConnected() const ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  // Tracks channel state for all endpoints. This includes what EndpointChannel
  // the endpoint is currently using and whether or not the EndpointChannel has
  // been encrypted yet.
  class ChannelState {
   public:
    struct EndpointData {
      EndpointData() = default;
      EndpointData(EndpointData&&) = default;
      EndpointData& operator=(EndpointData&&) = default;
      ~EndpointData() {
        if (channel != nullptr) {
          channel->Close(disconnect_reason);
        }
      }

      // True if we have a 'context' for the endpoint.
      bool IsEncrypted() const { return context != nullptr; }

      std::shared_ptr<EndpointChannel> channel;
      std::shared_ptr<EncryptionContext> context;
      location::nearby::proto::connections::DisconnectionReason
          disconnect_reason = location::nearby::proto::connections::
              DisconnectionReason::UNKNOWN_DISCONNECTION_REASON;
    };

    ChannelState() = default;
    ~ChannelState() { DestroyAll(); }
    ChannelState(ChannelState&&) = default;
    ChannelState& operator=(ChannelState&&) = default;

    // Provides a way to destroy contents of a container, while holding a lock.
    void DestroyAll() { endpoints_.clear(); }
    // Return pointer to endpoint data, or nullptr, it not found.
    EndpointData* LookupEndpointData(const std::string& endpoint_id);

    // Stores a new EndpointChannel for the endpoint.
    // Prevoius one is destroyed, if it existed.
    void UpdateChannelForEndpoint(const std::string& endpoint_id,
                                  std::unique_ptr<EndpointChannel> channel);

    // Stores a new EncryptionContext for the endpoint.
    // Prevoius one is destroyed, if it existed.
    void UpdateEncryptionContextForEndpoint(
        const std::string& endpoint_id,
        std::unique_ptr<EncryptionContext> context);

    // Removes all knowledge of this endpoint, cleaning up as necessary.
    // Returns false if the endpoint was not found.
    bool RemoveEndpoint(
        const std::string& endpoint_id,
        location::nearby::proto::connections::DisconnectionReason reason);

    bool EncryptChannel(EndpointData* endpoint);
    int GetConnectedEndpointsCount() const { return endpoints_.size(); }
    bool isWifiLanConnected() const;

   private:
    // Endpoint ID -> EndpointData. Contains everything we know about the
    // endpoint.
    absl::flat_hash_map<std::string, EndpointData> endpoints_;
  };

  void SetActiveEndpointChannel(ClientProxy* client,
                                const std::string& endpoint_id,
                                std::unique_ptr<EndpointChannel> channel,
                                bool enable_encryption)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  mutable Mutex mutex_;
  ChannelState channel_state_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_ENDPOINT_CHANNEL_MANAGER_H_
