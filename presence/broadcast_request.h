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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_BROADCAST_REQUEST_H_
#define THIRD_PARTY_NEARBY_PRESENCE_BROADCAST_REQUEST_H_

#include <stdint.h>

#include <string>

#include "absl/strings/string_view.h"
#include "absl/types/variant.h"
#include "presence/presence_identity.h"

namespace nearby {
namespace presence {

constexpr int8_t kUnspecifiedTxPower = -128;

/** Defines the action (intended actions) of base NP advertisement */
struct Action {
  uint16_t action;
};

/** Defines a Nearby Presence broadcast request */
struct BroadcastRequest {
  struct BasePresence {
    PresenceIdentity identity;
    Action action;
  };
  struct BaseFastPair {
    struct Discoverable {
      std::string model_id;
    };
    struct Nondiscoverable {
      std::string account_key_data;
      std::string battery_info;
    };
    absl::variant<Discoverable, Nondiscoverable> advertisement;
  };
  struct BaseEddystone {
    std::string ephemeral_id;
  };
  absl::variant<BasePresence, BaseFastPair, BaseEddystone> variant;
  std::string salt;
  int8_t tx_power;
  unsigned int interval_ms;
};

/** Builds a brodacast request variant with NP identity for BLE 4.2 */
class BasePresenceRequestBuilder {
 public:
  explicit BasePresenceRequestBuilder(const PresenceIdentity& identity)
      : identity_(identity) {}
  BasePresenceRequestBuilder& SetSalt(absl::string_view salt);
  BasePresenceRequestBuilder& SetTxPower(int8_t tx_power);
  BasePresenceRequestBuilder& SetAction(const Action& action);

  explicit operator BroadcastRequest() const;

 private:
  PresenceIdentity identity_;
  std::string salt_;
  int8_t tx_power_ = kUnspecifiedTxPower;
  Action action_;
};

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_BROADCAST_REQUEST_H_
