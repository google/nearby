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

#include <string>
#include <variant>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "internal/platform/logging.h"
#include "internal/platform/uuid.h"
#include "presence/data_element.h"

namespace nearby {
namespace presence {

constexpr uint8_t kBaseVersion = 0;
constexpr location::nearby::Uuid kServiceData(0xFCF1ULL << 32, 0);

absl::StatusOr<uint8_t> CreateDataElementHeader(size_t length,
                                                unsigned data_type) {
  if (length > DataElement::kMaxDataElementLength) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Unsupported Data Element length: %d", length));
  }
  if (data_type > DataElement::kMaxDataElementType) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Unsupported Data Element type: %d", data_type));
  }
  return (length << DataElement::kDataElementLengthShift) | data_type;
}

absl::Status AppendDataElement(unsigned data_type,
                               absl::string_view data_element,
                               std::string& output) {
  auto header = CreateDataElementHeader(data_element.size(), data_type);
  if (!header.ok()) {
    NEARBY_LOG(WARNING, "Can't add Data element type: %d, length: %d",
               data_type, data_element.size());
    return header.status();
  }
  output.push_back(*header);
  output.insert(output.end(), data_element.begin(), data_element.end());
  return absl::OkStatus();
}

uint8_t GetIdentityFieldType(PresenceIdentity::IdentityType type) {
  switch (type) {
    case PresenceIdentity::IdentityType::kPrivate:
      return DataElement::kPrivateIdentityFieldType;
    case PresenceIdentity::IdentityType::kTrusted:
      return DataElement::kTrustedIdentityFieldType;
    case PresenceIdentity::IdentityType::kPublic:
      return DataElement::kPublicIdentityFieldType;
    case PresenceIdentity::IdentityType::kProvisioned:  // fall-through
    default:
      return DataElement::kProvisionedIdentityFieldType;
  }
}

absl::StatusOr<BleAdvertisementData> AdvertisementFactory::CreateAdvertisement(
    const BroadcastRequest& request) const {
  BleAdvertisementData advert = {};
  if (std::holds_alternative<BroadcastRequest::BasePresence>(request.variant)) {
    return CreateBaseNpAdvertisement(request);
  }
  return advert;
}

absl::StatusOr<BleAdvertisementData>
AdvertisementFactory::CreateBaseNpAdvertisement(
    const BroadcastRequest& request) const {
  const auto& presence =
      std::get<BroadcastRequest::BasePresence>(request.variant);
  BleAdvertisementData advert{};
  std::string payload;
  payload.push_back(kBaseVersion);
  absl::Status result;
  if (!request.salt.empty()) {
    result =
        AppendDataElement(DataElement::kSaltFieldType, request.salt, payload);
    if (!result.ok()) {
      return result;
    }
  }
  absl::StatusOr<std::string> identity =
      credential_manager_.GetBaseEncryptedMetadataKey(presence.identity);
  if (!identity.ok()) {
    return identity.status();
  }
  uint8_t identity_type =
      GetIdentityFieldType(presence.identity.GetIdentityType());
  result = AppendDataElement(identity_type, *identity, payload);
  if (!result.ok()) {
    return result;
  }

  std::string data_elements;
  std::string tx_power = {static_cast<char>(request.tx_power)};
  result = AppendDataElement(DataElement::kTxPowerFieldType, tx_power,
                             data_elements);
  if (!result.ok()) {
    return result;
  }
  std::string action = {static_cast<char>(presence.action.action >> 8),
                        static_cast<char>(presence.action.action)};
  result =
      AppendDataElement(DataElement::kActionFieldType, action, data_elements);
  if (!result.ok()) {
    return result;
  }
  if (!identity->empty()) {
    auto encrypted = credential_manager_.EncryptDataElements(
        presence.identity, request.salt, data_elements);
    if (!encrypted.ok()) {
      return encrypted.status();
    }
    payload += *encrypted;
  } else {
    payload += data_elements;
  }
  advert.service_data.insert(
      {kServiceData, location::nearby::ByteArray(payload)});
  return advert;
}

}  // namespace presence
}  // namespace nearby
