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


#include <utility>
#include <vector>

#include "absl/status/statusor.h"
#include "presence/data_types.h"
#include "presence/presence_device.h"
#include "presence/scan_request.h"

namespace nearby {
namespace presence {

absl::StatusOr<ScanSessionId> FakePresenceClient::StartScan(
    ScanRequest scan_request, ScanCallback callback) {
  uint64_t id_value = 1;
  absl::StatusOr<ScanSessionId> scan_session_id(id_value);
  callback_ = std::move(callback);
  return scan_session_id;
}

void FakePresenceClient::StopScan(ScanSessionId id) {}

std::vector<uint64_t> FakePresenceClient::GetActiveScanSessions() {
  return active_scan_sessions_;
}

void FakePresenceClient::CallStartScanCallbackOk() {
  callback_.start_scan_cb(absl::OkStatus());
}

void FakePresenceClient::CallStartScanCallbackCancelled() {
  absl::Status status(absl::StatusCode::kCancelled, "");
  callback_.start_scan_cb(status);
}

void FakePresenceClient::CallOnDiscovered() {
  ::nearby::internal::Metadata metadata;
  metadata.set_bluetooth_mac_address("01234567");
  metadata.set_device_name("Pepper's device");
  PresenceDevice device{metadata};

  callback_.on_discovered_cb(std::move(device));
}

void FakePresenceClient::CallOnUpdated() {
  ::nearby::internal::Metadata metadata;
  metadata.set_bluetooth_mac_address("01234567");
  metadata.set_device_name("Pepper's device");
  PresenceDevice device{metadata};
  callback_.on_updated_cb(std::move(device));
}

void FakePresenceClient::CallOnLost() {
  ::nearby::internal::Metadata metadata;
  metadata.set_bluetooth_mac_address("01234567");
  metadata.set_device_name("Pepper's device");
  PresenceDevice device{metadata};
  callback_.on_lost_cb(std::move(device));
}

}  // namespace presence
}  // namespace nearby
