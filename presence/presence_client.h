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

#include "presence/broadcast_request.h"
#include "presence/data_types.h"
#include "presence/scan_request.h"

namespace nearby {
namespace presence {

/**
 * Interface for detecting and interacting with nearby devices that are also
 * part of the Presence ecosystem.
 */
class PresenceClient {
 public:
  /**
   * Starts a Nearby Presence scan and registers {@link ScanCallback}
   * which will be invoked when a matching {@link PresenceDevice} is detected,
   * lost, and status changed.
   *
   * <p>The {@link ScanCallback} is kept at the Nearby Presence service.
   * Returning unique_ptr of ScanSession including stop scan callback
   * for client to invoke later.
   *
   * <p>The {@link ScanRequest} contains the options like scan power mode
   * and type; the filters including credentials, actions and extended
   * properties.
   */

  std::unique_ptr<ScanSession> StartScan(ScanRequest scan_request,
                                         ScanCallback callback);

  /**
   * Starts a Nearby Presence broadcast and registers {@link BroadcastCallback}
   * which will be invoked after broadcast is started.
   *
   * <p>The {@link BroadcastCallback} is kept at the Nearby Presence service.
   * Returning unique_ptr of BroadcastSession including stop broadcast callback
   * for the client to invoke later.
   *
   * <p>The {@link BroadcastRequest} contains the options like tx_power,
   * the credential info like salt and private credential, the actions and
   * extended properties.
   */
  std::unique_ptr<BroadcastSession> StartBroadcast(
      BroadcastRequest broadcast_request, BroadcastCallback callback);
};

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_PRESENCE_CLIENT_H_
