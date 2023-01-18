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

#include "presence/implementation/service_controller_impl.h"

#include <utility>

#include "absl/status/statusor.h"
#include "presence/data_types.h"

namespace nearby {
namespace presence {

absl::StatusOr<ScanSessionId> ServiceControllerImpl::StartScan(
    ScanRequest scan_request, ScanCallback callback) {
  return scan_manager_.StartScan(scan_request, std::move(callback));
}
void ServiceControllerImpl::StopScan(ScanSessionId id) {
  scan_manager_.StopScan(id);
}

absl::StatusOr<BroadcastSessionId> ServiceControllerImpl::StartBroadcast(
    BroadcastRequest broadcast_request, BroadcastCallback callback) {
  return broadcast_manager_.StartBroadcast(broadcast_request,
                                           std::move(callback));
}

void ServiceControllerImpl::StopBroadcast(BroadcastSessionId id) {
  broadcast_manager_.StopBroadcast(id);
}

}  // namespace presence
}  // namespace nearby
