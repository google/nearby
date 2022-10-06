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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_SCAN_CALLBACK_H_
#define THIRD_PARTY_NEARBY_PRESENCE_SCAN_CALLBACK_H_

#include <functional>

#include "presence/presence_device.h"
#include "presence/status.h"

namespace nearby {
namespace presence {

// Holds the callback of stop scan for client to invoke later.
struct ScanSession {
  std::function<void(Status)> stop_scan_callback;
};

struct ScanCallback {
  // Updates client with the result of start scanning.
  std::function<void(Status)> start_scan_cb;

  // Reports a {@link PresenceDevice} being discovered.
  std::function<void(PresenceDevice)> on_discovered_cb;

  // Reports a {@link PresenceDevice} information(distance, and etc)
  // changed.
  std::function<void(PresenceDevice)> on_updated_cb;

  // Reports a {@link PresenceDevice} is no longer within range.
  std::function<void(PresenceDevice)> on_lost_cb;
};

/**
 * Holds the callback of stop broadcast for client to invoke later.
 */
struct BroadcastSession {
  std::function<void(Status)> stop_broadcast_callback;
};

struct BroadcastCallback {
  std::function<void(Status)> start_broadcast_cb;
};

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_SCAN_CALLBACK_H_
