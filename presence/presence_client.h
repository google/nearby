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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_PRESENCE_CLIENT_H_
#define THIRD_PARTY_NEARBY_PRESENCE_PRESENCE_CLIENT_H_

#include <functional>
#include <memory>
#include <vector>

#include "absl/status/statusor.h"
#include "internal/platform/borrowable.h"
#include "presence/broadcast_request.h"
#include "presence/data_types.h"
#include "presence/scan_request.h"

namespace nearby {
namespace presence {

class PresenceService;
/**
 * Interface for detecting and interacting with nearby devices that are also
 * part of the Presence ecosystem.
 */
class PresenceClient {
 public:
  using BorrowablePresenceService = ::nearby::Borrowable<PresenceService*>;

  explicit PresenceClient(BorrowablePresenceService service)
      : service_(service) {}
  PresenceClient(const PresenceClient&) = delete;
  PresenceClient(PresenceClient&&) = default;
  PresenceClient& operator=(const PresenceClient&) = delete;

  // Starts a Nearby Presence scan and registers `ScanCallback`
  // which will be invoked when a matching `PresenceDevice` is detected,
  // lost, and status changed.
  // The session can be terminated with `StopScan()`.
  //
  // `ScanCallback` is kept in the Nearby Presence service until `StopScan()` is
  // called.
  //
  // `ScanRequest` contains the options like scan power mode
  // and type; the filters including credentials, actions and extended
  // properties.
  absl::StatusOr<ScanSessionId> StartScan(ScanRequest scan_request,
                                          ScanCallback callback);

  // Terminates the scan session. Does nothing if the session is already
  // terminated.
  void StopScan(ScanSessionId session_id);

  // Starts a Nearby Presence broadcast and registers `BroadcastCallback`
  // which will be invoked after broadcast is started.
  // The session can be terminated with `StopBroadcast()`.
  //
  // `BroadcastCallback` is kept in the Nearby Presence service until
  // `StopBroadcast()` is called.
  //
  // `BroadcastRequest` contains the options like tx_power,
  // the credential info like salt and private credential, the actions and
  // extended properties.
  absl::StatusOr<BroadcastSessionId> StartBroadcast(
      BroadcastRequest broadcast_request, BroadcastCallback callback);

  // Terminates a broadcast session. Does nothing if the session is already
  // terminated.
  void StopBroadcast(BroadcastSessionId session_id);

 private:
  BorrowablePresenceService service_;
};

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_PRESENCE_CLIENT_H_
