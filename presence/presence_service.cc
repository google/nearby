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

#include "presence/presence_service.h"

#include <memory>
#include <optional>

#include "presence/data_types.h"
#include "presence/implementation/service_controller_impl.h"

namespace nearby {
namespace presence {
PresenceService::PresenceService() {
  this->service_controller_ = std::make_unique<ServiceControllerImpl>();
}

PresenceClient PresenceService::CreatePresenceClient() {
  return PresenceClient(this);
}

std::optional<ScanSession> PresenceService::StartScan(
    ScanRequest scan_request, ScanCallback callback) {
  return service_controller_->StartScan(scan_request, callback);
}

std::unique_ptr<BroadcastSession> PresenceService::StartBroadcast(
    BroadcastRequest broadcast_request, BroadcastCallback callback) {
  return service_controller_->StartBroadcast(broadcast_request, callback);
}

}  // namespace presence
}  // namespace nearby
