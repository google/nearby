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

#include "presence/implementation/base_broadcast_request.h"

#include <string>
#include <variant>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "internal/platform/implementation/crypto.h"
#include "internal/platform/logging.h"
#include "presence/broadcast_request.h"
#include "presence/implementation/action_factory.h"

namespace nearby {
namespace presence {

BasePresenceRequestBuilder& BasePresenceRequestBuilder::SetSalt(
    absl::string_view salt) {
  if (salt.size() != kSaltSize) {
    NEARBY_LOG(WARNING, "Unsupported salt length: %d", salt.size());
  } else {
    salt_ = std::string(salt);
  }
  return *this;
}
BasePresenceRequestBuilder& BasePresenceRequestBuilder::SetTxPower(
    int8_t tx_power) {
  tx_power_ = tx_power;
  return *this;
}

BasePresenceRequestBuilder& BasePresenceRequestBuilder::SetAction(
    const Action& action) {
  action_ = action;
  return *this;
}

BasePresenceRequestBuilder& BasePresenceRequestBuilder::SetPowerMode(
    PowerMode power_mode) {
  power_mode_ = power_mode;
  return *this;
}

BasePresenceRequestBuilder& BasePresenceRequestBuilder::SetAccountName(
    absl::string_view account_name) {
  account_name_ = std::string(account_name);
  return *this;
}

BasePresenceRequestBuilder& BasePresenceRequestBuilder::SetManagerAppId(
    absl::string_view manager_app_id) {
  manager_app_id_ = std::string(manager_app_id);
  return *this;
}

BasePresenceRequestBuilder::operator BaseBroadcastRequest() const {
  BaseBroadcastRequest::BasePresence presence{
      .credential_selector = {.manager_app_id = manager_app_id_,
                              .account_name = account_name_,
                              .identity_type = identity_},
      .action = action_};

  std::string bytes(kSaltSize, 0);
  crypto::RandBytes(const_cast<std::string::value_type*>(bytes.data()),
                    bytes.size());

  BaseBroadcastRequest broadcast_request{
      .variant = presence,
      .salt = salt_.size() == kSaltSize ? salt_ : bytes,
      .tx_power = tx_power_,
      .power_mode = power_mode_};
  return broadcast_request;
}

absl::StatusOr<BaseBroadcastRequest> BaseBroadcastRequest::Create(
    const BroadcastRequest& request) {
  if (absl::holds_alternative<PresenceBroadcast>(request.variant)) {
    const auto& presence_request =
        absl::get<PresenceBroadcast>(request.variant);
    if (presence_request.sections.empty()) {
      return absl::InvalidArgumentError("Missing broadcast sections");
    }
    if (presence_request.sections.size() > 1) {
      NEARBY_LOG(WARNING,
                 "Only first section is used in BLE 4.2 advertisement");
    }
    const PresenceBroadcast::BroadcastSection& section =
        presence_request.sections.front();
    return BaseBroadcastRequest(
        BasePresenceRequestBuilder(section.identity)
            .SetTxPower(request.tx_power)
            .SetAction(ActionFactory::CreateAction(section.extended_properties))
            .SetPowerMode(request.power_mode)
            .SetManagerAppId(section.manager_app_id)
            .SetAccountName(section.account_name));
  }
  return absl::UnimplementedError("Request not supported");
}

}  // namespace presence
}  // namespace nearby
