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

#include "presence/presence_client.h"

#include <memory>
#include <optional>
#include <vector>

#include "absl/log/die_if_null.h"
#include "presence/presence_service.h"

namespace nearby {
namespace presence {

PresenceClient::PresenceClient(PresenceService* service)
    : service_(*ABSL_DIE_IF_NULL(service)) {}

std::optional<ScanSession> PresenceClient::StartScan(ScanRequest scan_request,
                                                       ScanCallback callback) {
  return service_.StartScan(scan_request, callback);
}
std::unique_ptr<BroadcastSession> PresenceClient::StartBroadcast(
    BroadcastRequest broadcast_request, BroadcastCallback callback) {
  return service_.StartBroadcast(broadcast_request, callback);
}

}  // namespace presence
}  // namespace nearby
