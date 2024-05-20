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

#include "presence/implementation/advertisement_factory.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "nearby_protocol.h"
#include "internal/platform/implementation/credential_callbacks.h"
#include "internal/platform/logging.h"
#include "internal/proto/credential.pb.h"
#include "presence/data_element.h"
#include "presence/implementation/base_broadcast_request.h"
#include "presence/implementation/mediums/advertisement_data.h"

namespace nearby {
namespace presence {

namespace {
using ::nearby::internal::IdentityType;
constexpr uint8_t kBaseVersion = 0;

bool IsEncryptedAdvertisement(IdentityType identity_type) {
  return identity_type == IdentityType::IDENTITY_TYPE_PRIVATE_GROUP ||
         identity_type == IdentityType::IDENTITY_TYPE_CONTACTS_GROUP;
}

absl::StatusOr<nearby_protocol::V0BroadcastCredential>
CreateV0BroadcastCredential(std::string key_seed, std::string identity_token) {
  std::array<uint8_t, 32> key_seed_bytes;
  std::copy(key_seed.begin(), key_seed.end(), key_seed_bytes.data());

  std::array<uint8_t, 14> identity_token_bytes;
  std::copy(identity_token.begin(), identity_token.end(),
            identity_token_bytes.data());

  return nearby_protocol::V0BroadcastCredential(key_seed_bytes,
                                                identity_token_bytes);
}

absl::StatusOr<nearby_protocol::ActionType> MapAction(const ActionBit action) {
  switch (action) {
    case ActionBit::kActiveUnlockAction:
      return nearby_protocol::ActionType::ActiveUnlock;
    case ActionBit::kNearbyShareAction:
      return nearby_protocol::ActionType::NearbyShare;
    case ActionBit::kInstantTetheringAction:
      return nearby_protocol::ActionType::InstantTethering;
    case ActionBit::kPhoneHubAction:
      return nearby_protocol::ActionType::PhoneHub;
    default:
      return absl::InvalidArgumentError("Unsupported action type");
  }
}

nearby_protocol::V0DataElement CreateActionsDataElement(
    const std::vector<DataElement> data_elements,
    nearby_protocol::AdvertisementBuilderKind kind) {
  auto actions = nearby_protocol::V0Actions::BuildNewZeroed(kind);
  for (const auto& data_element : data_elements) {
    if (data_element.GetType() == data_element.kActionFieldType) {
      auto action = static_cast<ActionBit>(data_element.GetValue()[0]);
      auto action_type = MapAction(action);
      if (!action_type.ok()) {
        NEARBY_LOGS(WARNING)
            << "Unsupported action type, is not being added to the broadcast: "
            << (int)action;
        continue;
      }
      auto result = actions.TrySetAction(*action_type, true);
      if (!result.ok()) {
        auto kind_string =
            (kind == nearby_protocol::AdvertisementBuilderKind::Public)
                ? "Plaintext"
                : "Encrypted";
        NEARBY_LOGS(WARNING) << "Provided action type is not supported for "
                                "given advertisement type: "
                             << kind_string << " action:" << (int)action;
        continue;
      }
    }
  }
  return nearby_protocol::V0DataElement(actions);
}

absl::StatusOr<AdvertisementData> CreateEncryptedAdvertisement(
    const BaseBroadcastRequest& request, internal::LocalCredential credential) {
  if (request.salt.size() != kSaltSize) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Unsupported salt size %d", request.salt.size()));
  }
  std::array<uint8_t, kSaltSize> salt;
  std::copy(request.salt.begin(), request.salt.end(), salt.data());

  auto broadcast_credential = CreateV0BroadcastCredential(
      credential.key_seed(), credential.identity_token_v0());
  if (!broadcast_credential.ok()) {
    return broadcast_credential.status();
  }

  auto adv_builder = nearby_protocol::V0AdvertisementBuilder::CreateEncrypted(
      *broadcast_credential, salt);

  auto tx_power = nearby_protocol::TxPower::TryBuildFromI8(request.tx_power);
  if (!tx_power.ok()) {
    return tx_power.status();
  }

  auto tx_power_data_element = nearby_protocol::V0DataElement(tx_power.value());
  auto result = adv_builder.TryAddDE(tx_power_data_element);
  if (!result.ok()) {
    return result;
  }

  if (!request.data_elements.empty()) {
    auto actions_data_element = CreateActionsDataElement(
        request.data_elements,
        nearby_protocol::AdvertisementBuilderKind::Encrypted);
    auto add_de_result = adv_builder.TryAddDE(actions_data_element);
    if (!add_de_result.ok()) {
      return add_de_result;
    }
  }

  auto serialized_bytes = adv_builder.TrySerialize();
  if (!serialized_bytes.ok()) {
    return serialized_bytes.status();
  }

  return AdvertisementData{.is_extended_advertisement = false,
                           .content = serialized_bytes->ToString()};
}

absl::StatusOr<AdvertisementData> CreateUnencryptedAdvertisement(
    const BaseBroadcastRequest& request) {
  auto adv_builder = nearby_protocol::V0AdvertisementBuilder::CreatePublic();

  auto tx_power = nearby_protocol::TxPower::TryBuildFromI8(request.tx_power);
  if (!tx_power.ok()) {
    return tx_power.status();
  }
  auto de = nearby_protocol::V0DataElement(*tx_power);
  auto add_de_result = adv_builder.TryAddDE(de);
  if (!add_de_result.ok()) {
    return add_de_result;
  }

  if (!request.data_elements.empty()) {
    auto actions_data_element = CreateActionsDataElement(
        request.data_elements,
        nearby_protocol::AdvertisementBuilderKind::Public);
    add_de_result = adv_builder.TryAddDE(actions_data_element);
    if (!add_de_result.ok()) {
      return add_de_result;
    }
  }

  auto serialized_bytes = adv_builder.TrySerialize();
  if (!serialized_bytes.ok()) {
    return serialized_bytes.status();
  }
  return AdvertisementData{.is_extended_advertisement = false,
                           .content = serialized_bytes->ToString()};
}

}  // namespace

absl::StatusOr<AdvertisementData> AdvertisementFactory::CreateAdvertisement(
    const BaseBroadcastRequest& request,
    absl::optional<LocalCredential> credential) const {  // NOLINT
  if (IsEncryptedAdvertisement(request.credential_selector.identity_type)) {
    if (!credential) {
      return absl::FailedPreconditionError("Missing credentials");
    }
    return CreateEncryptedAdvertisement(request, *credential);
  } else {
    return CreateUnencryptedAdvertisement(request);
  }
}

absl::StatusOr<CredentialSelector> AdvertisementFactory::GetCredentialSelector(
    const BaseBroadcastRequest& request) {
  if (IsEncryptedAdvertisement(request.credential_selector.identity_type)) {
    return request.credential_selector;
  }
  return absl::NotFoundError("credentials not required");
}

}  // namespace presence
}  // namespace nearby
