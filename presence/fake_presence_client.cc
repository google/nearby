// Copyright 2023 Google LLC
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

#include "presence/fake_presence_client.h"


#include <algorithm>
#include <utility>
#include <vector>

#include "presence/data_types.h"
#include "presence/presence_device.h"
#include "presence/scan_request.h"

namespace nearby {
namespace presence {

absl::StatusOr<ScanSessionId> FakePresenceClient::StartScan(
    ScanRequest scan_request, ScanCallback callback) {
  current_scan_session_id_++;
  active_scan_sessions_.push_back(current_scan_session_id_);
  absl::StatusOr<ScanSessionId> scan_session_id(current_scan_session_id_);
  callback_ = std::move(callback);
  return scan_session_id;
}

void FakePresenceClient::StopScan(ScanSessionId id) {
  auto position =
      std::find(active_scan_sessions_.begin(), active_scan_sessions_.end(), id);
  if (position != active_scan_sessions_.end()) {
    active_scan_sessions_.erase(position);
  }
}

std::vector<uint64_t> FakePresenceClient::GetActiveScanSessions() {
  return active_scan_sessions_;
}

void FakePresenceClient::CallStartScanCallback(absl::Status status) {
  callback_.start_scan_cb(status);
}

void FakePresenceClient::CallOnDiscovered(PresenceDevice device) {
  callback_.on_discovered_cb(device);
}

void FakePresenceClient::CallOnUpdated(PresenceDevice device) {
  callback_.on_updated_cb(device);
}

void FakePresenceClient::CallOnLost(PresenceDevice device) {
  callback_.on_lost_cb(device);
}

}  // namespace presence
}  // namespace nearby
