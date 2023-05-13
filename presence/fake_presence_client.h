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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_FAKE_PRESENCE_CLIENT_H_
#define THIRD_PARTY_NEARBY_PRESENCE_FAKE_PRESENCE_CLIENT_H_


#include <optional>
#include <vector>

#include "absl/status/statusor.h"
#include "presence/broadcast_request.h"
#include "presence/presence_client.h"
#include "presence/presence_device.h"
#include "presence/scan_request.h"

namespace nearby {
namespace presence {

/**
 * Interface for detecting and interacting with nearby devices that are also
 * part of the Presence ecosystem.
 */
class FakePresenceClient : public PresenceClient {
 public:
  explicit FakePresenceClient(BorrowablePresenceService service)
      : PresenceClient(service) {}
  FakePresenceClient(const FakePresenceClient&) = delete;
  FakePresenceClient(FakePresenceClient&&) = default;
  FakePresenceClient& operator=(const FakePresenceClient&) = delete;
  ~FakePresenceClient() override = default;

  // Starts a Nearby Presence scan and registers `ScanCallback`
  absl::StatusOr<ScanSessionId> StartScan(ScanRequest scan_request,
                                          ScanCallback callback) override;

  // Terminates the scan session. Does nothing if the session is already
  // terminated.
  void StopScan(ScanSessionId session_id) override;

  absl::StatusOr<BroadcastSessionId> StartBroadcast(
      BroadcastRequest broadcast_request, BroadcastCallback callback) override {
    absl::StatusOr<int> thing;
    return thing;
  }

  void StopBroadcast(BroadcastSessionId session_id) override {}

  std::optional<PresenceDevice> GetLocalDevice() override {
    return std::nullopt;
  }

  void SetNextScanSessionSuccess(bool success) {
    next_scan_should_succeed_ = success;
  }

  std::vector<uint64_t> GetActiveScanSessions();
  void CallStartScanCallbackOk();
  void CallStartScanCallbackCancelled();
  void CallOnDiscovered();
  void CallOnUpdated();
  void CallOnLost();

 private:
  ScanCallback callback_;
  std::vector<uint64_t> active_scan_sessions_;
  bool next_scan_should_succeed_ = true;
};

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_FAKE_PRESENCE_CLIENT_H_
