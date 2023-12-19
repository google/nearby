// Copyright 2022 Google LLC
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

#ifndef CORE_INTERNAL_SERVICE_ID_CONSTANTS_H_
#define CORE_INTERNAL_SERVICE_ID_CONSTANTS_H_

#include <string>

#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"

namespace nearby {
namespace connections {

constexpr absl::string_view kUnknownServiceId = "UNKNOWN_SERVICE";

// A suffix appended to service IDs when initiating a bandwidth upgrade to
// distinguish the mediums from those used for advertising/discovery.
constexpr absl::string_view kInitiatorUpgradeServiceIdPostfix = "_UPGRADE";
constexpr absl::string_view kInitiatorReconnectServiceIdPostfix = "_RECONNECT";

// Returns true if |service_id| not empty and has the initiator's upgrade
// postfix.
inline bool IsInitiatorUpgradeServiceId(absl::string_view service_id) {
  return !service_id.empty() &&
         absl::EndsWith(service_id, kInitiatorUpgradeServiceIdPostfix);
}

// Appends the kInitiatorUpgradeServiceIdPostfix to |service_id| if necessary.
inline std::string WrapInitiatorUpgradeServiceId(absl::string_view service_id) {
  // If |service_id| is empty or already has the upgrade postfix, do nothing.
  if (service_id.empty() || IsInitiatorUpgradeServiceId(service_id)) {
    return std::string(service_id);
  }

  return std::string(service_id) +
         std::string(kInitiatorUpgradeServiceIdPostfix);
}

// Returns true if |service_id| not empty and has the initiator's reconnect
// postfix.
inline bool IsInitiatorReconnectServiceId(absl::string_view service_id) {
  return !service_id.empty() &&
         absl::EndsWith(service_id, kInitiatorReconnectServiceIdPostfix);
}

// Appends the kInitiatorReconnectServiceIdPostfix to |service_id| if necessary.
inline std::string WrapInitiatorReconnectServiceId(
    absl::string_view service_id) {
  // If |service_id| is empty or already has the reconnect postfix, do nothing.
  if (service_id.empty() || IsInitiatorReconnectServiceId(service_id)) {
    return std::string(service_id);
  }

  return std::string(service_id) +
         std::string(kInitiatorReconnectServiceIdPostfix);
}

// Appends the kInitiatorReconnectServiceIdPostfix to |service_id| if necessary.
inline std::string UnWrapInitiatorReconnectServiceId(
    absl::string_view service_id) {
  // If |service_id| is empty or already has the reconnect postfix, do nothing.
  if (service_id.empty() || !IsInitiatorReconnectServiceId(service_id)) {
    return std::string(service_id);
  }

  return std::string(
      absl::StripSuffix(service_id, kInitiatorReconnectServiceIdPostfix));
}

}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_SERVICE_ID_CONSTANTS_H_
