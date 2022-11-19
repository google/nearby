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
#include <vector>

#include "internal/platform/borrowable.h"
#include "presence/presence_service.h"

namespace nearby {
namespace presence {

std::unique_ptr<ScanSession> PresenceClient::StartScan(ScanRequest scan_request,
                                                       ScanCallback callback) {
  ::location::nearby::Borrowed<PresenceService*> borrowed = service_.Borrow();
  if (!borrowed) {
    return nullptr;
  }
  return (*borrowed)->StartScan(scan_request, callback);
}
std::unique_ptr<BroadcastSession> PresenceClient::StartBroadcast(
    BroadcastRequest broadcast_request, BroadcastCallback callback) {
  ::location::nearby::Borrowed<PresenceService*> borrowed = service_.Borrow();
  if (!borrowed) {
    return nullptr;
  }
  return (*borrowed)->StartBroadcast(broadcast_request, callback);
}

}  // namespace presence
}  // namespace nearby
