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

#include <optional>

#include "absl/status/statusor.h"
#include "internal/platform/borrowable.h"
#include "presence/broadcast_request.h"
#include "presence/data_types.h"
#include "presence/presence_device.h"
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

  virtual ~PresenceClient() = default;

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
  virtual absl::StatusOr<ScanSessionId> StartScan(ScanRequest scan_request,
                                          ScanCallback callback) = 0;

  // Terminates the scan session. Does nothing if the session is already
  // terminated.
  virtual void StopScan(ScanSessionId session_id) = 0;

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
  virtual absl::StatusOr<BroadcastSessionId> StartBroadcast(
      BroadcastRequest broadcast_request, BroadcastCallback callback) = 0;

  // Terminates a broadcast session. Does nothing if the session is already
  // terminated.
  virtual void StopBroadcast(BroadcastSessionId session_id) = 0;

  // Returns the local PresenceDevice describing the current device's actions,
  // connectivity info and unique identifier for use in Connections and
  // Presence.
  virtual std::optional<PresenceDevice> GetLocalDevice() = 0;
};

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_PRESENCE_CLIENT_H_
