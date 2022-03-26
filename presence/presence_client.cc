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

#include "third_party/nearby/presence/presence_client.h"

#include "third_party/nearby/presence/presence_device.h"

namespace nearby {
namespace presence {

void PresenceClient::StartDiscovery(const DiscoveryFilter& filter,
                                    const DiscoveryOptions& options,
                                    ResultCallback callback) {}

void PresenceClient::UpdateDiscoveryFilter(const DiscoveryFilter& filter,
                                           ResultCallback callback) {}

void PresenceClient::StopDiscovery(ResultCallback callback) {}

std::vector<PresenceDevice> PresenceClient::GetCachedDevices(
    const DiscoveryFilter& filter) {
  return std::vector<PresenceDevice>{};
}

void PresenceClient::StartBroadcast(const PresenceIdentity& identity,
                                    const std::vector<PresenceAction>& actions,
                                    const BroadcastOptions& options,
                                    ResultCallback callback) {}

void PresenceClient::UpdateBroadcastActions(
    const PresenceIdentity& identity,
    const std::vector<PresenceAction>& actions, ResultCallback callback) {}

void PresenceClient::StopBroadcast(const PresenceIdentity& identity,
                                   ResultCallback callback) {}

}  // namespace presence
}  // namespace nearby
