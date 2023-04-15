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
#include "internal/platform/logging.h"
#include "presence/presence_device.h"

namespace nearby {
namespace presence {

// Unique Scan Session Identifier.
using ScanSessionId = uint64_t;

// Callers would provide the implementation of these callbacks. If callers
// don't need these signal updates, they can skip with the provided default
// empty functions.
struct ScanCallback {
  // Updates client with the result of start scanning.
  absl::AnyInvocable<void(absl::Status)> start_scan_cb = [](absl::Status) {};

  // Reports a {@link PresenceDevice} being discovered.
  absl::AnyInvocable<void(PresenceDevice)> on_discovered_cb =
      [](PresenceDevice) {};

  // Reports a {@link PresenceDevice} information(distance, and etc)
  // changed.
  absl::AnyInvocable<void(PresenceDevice)> on_updated_cb = [](PresenceDevice) {
  };

  // Reports a {@link PresenceDevice} is no longer within range.
  absl::AnyInvocable<void(PresenceDevice)> on_lost_cb = [](PresenceDevice) {};
};

// Unique Broadcast Session Identifier.
using BroadcastSessionId = uint64_t;

// Callers would provide the implementation of these callbacks. If callers
// don't need these signal updates, they can skip with the provided default
// empty functions.
struct BroadcastCallback {
  absl::AnyInvocable<void(absl::Status)> start_broadcast_cb = [](absl::Status) {
  };
};

// Chromium uses its own crypto library instead of nearby/internal/crypto,
// in which base::span is used instead of absl::Span. See b/276368162.
#ifdef NEARBY_CHROMIUM
template <typename T>
using CryptoSpan = base::span<T>;
#else
template <typename T>
using CryptoSpan = absl::Span<T>;
#endif
}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_SCAN_CALLBACK_H_
