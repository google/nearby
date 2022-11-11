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
#include <utility>

#include "absl/functional/any_invocable.h"
#include "presence/presence_device.h"
#include "presence/status.h"

namespace nearby {
namespace presence {

// Holds the callback of stop scan for client to invoke later.
// TODO(b/254895067) Rework status for absl::Status
class ScanSession {
 public:
  ScanSession()
      : stop_scan_callback_(
            []() { return Status{Status::Value::kNotImplemented}; }) {}
  explicit ScanSession(absl::AnyInvocable<Status(void) &&> stop_scan_callback)
      : stop_scan_callback_(std::move(stop_scan_callback)) {}

  ~ScanSession() { StopScan(); }

  Status StopScan() {
    if (stop_called_) {
      return Status{Status::Value::kError};
    }
    stop_called_ = true;
    auto callback = std::move(stop_scan_callback_);
    if (callback) {
      return std::move(callback)();
    }
    return Status{Status::Value::kError};
  }

 private:
  // Nearby library would provide the implementation of this callback in
  // runtime. Assigning with a default value NotImplemented to surface potential
  // issue where library failed to provide the implementation.
  absl::AnyInvocable<Status(void) &&> stop_scan_callback_;
  bool stop_called_ = false;
};

// Callers would provide the implementation of these callbacks. If callers
// don't need these signal updates, they can skip with the provided default
// empty functions.
struct ScanCallback {
  // Updates client with the result of start scanning.
  std::function<void(Status)> start_scan_cb = [](Status) {};

  // Reports a {@link PresenceDevice} being discovered.
  std::function<void(PresenceDevice)> on_discovered_cb = [](PresenceDevice) {};

  // Reports a {@link PresenceDevice} information(distance, and etc)
  // changed.
  std::function<void(PresenceDevice)> on_updated_cb = [](PresenceDevice) {};

  // Reports a {@link PresenceDevice} is no longer within range.
  std::function<void(PresenceDevice)> on_lost_cb = [](PresenceDevice) {};
};

/**
 * Holds the callback of stop broadcast for client to invoke later.
 */
struct BroadcastSession {
  // Nearby library would provide the implementation of this callback in
  // runtime. Assiging with a default value NotImplemented to surface potential
  // issue where library failed to provide the implementation.
  std::function<Status(void)> stop_broadcast_callback = []() {
    return Status{Status::Value::kNotImplemented};
  };
};

// Callers would provide the implementation of these callbacks. If callers
// don't need these signal updates, they can skip with the provided default
// empty functions.
struct BroadcastCallback {
  std::function<void(Status)> start_broadcast_cb = [](Status) {};
};

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_SCAN_CALLBACK_H_
