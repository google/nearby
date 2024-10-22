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

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/status/status.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "internal/platform/implementation/credential_callbacks.h"
#include "internal/platform/logging.h"
#include "internal/platform/uuid.h"
#include "internal/proto/credential.pb.h"
#include "presence/data_element.h"
#include "presence/implementation/base_broadcast_request.h"
#include "presence/implementation/ldt.h"
#include "presence/implementation/mediums/advertisement_data.h"

namespace nearby {
namespace presence {

namespace {
using ::nearby::internal::IdentityType;
constexpr uint8_t kBaseVersion = 0;
constexpr size_t kMaxBaseNpAdvSize = 26;

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
    NEARBY_LOGS(WARNING) << "Can't add Data element type: " << data_type
                         << ", length: " << data_element.size();
    return header.status();
  }
  output.push_back(*header);
  output.insert(output.end(), data_element.begin(), data_element.end());
  return absl::OkStatus();
}

uint8_t GetIdentityFieldType(IdentityType type) {
  switch (type) {
    case IdentityType::IDENTITY_TYPE_PRIVATE_GROUP:
      return DataElement::kPrivateGroupIdentityFieldType;
    case IdentityType::IDENTITY_TYPE_CONTACTS_GROUP:
      return DataElement::kContactsGroupIdentityFieldType;
    case IdentityType::IDENTITY_TYPE_PUBLIC:
      ABSL_FALLTHROUGH_INTENDED;
    default:
      return DataElement::kPublicIdentityFieldType;
  }
}

std::string SerializeAction(const Action& action) {
  std::string output;
  uint32_t input = action.action;
  for (int i = 3; i >= 0; --i) {
    if (input == 0) {
      return output;
    }
    int shift = 8 * i;
    output.push_back(static_cast<char>((input >> shift) & 0xFF));
    input &= (1 << shift) - 1;
  }
  return output;
}

bool RequiresCredentials(IdentityType identity_type) {
  return identity_type == IdentityType::IDENTITY_TYPE_PRIVATE_GROUP ||
         identity_type == IdentityType::IDENTITY_TYPE_CONTACTS_GROUP;
}
}  // namespace

absl::StatusOr<AdvertisementData> AdvertisementFactory::CreateAdvertisement(
    const BaseBroadcastRequest& request,
    absl::optional<LocalCredential> credential) const {
  AdvertisementData advert = {};
  if (absl::holds_alternative<BaseBroadcastRequest::BasePresence>(
          request.variant)) {
    return CreateBaseNpAdvertisement(request, std::move(credential));
  }
  return advert;
}

absl::StatusOr<AdvertisementData>
AdvertisementFactory::CreateBaseNpAdvertisement(
    const BaseBroadcastRequest& request,
    absl::optional<LocalCredential> credential) const {
  const auto& presence =
      absl::get<BaseBroadcastRequest::BasePresence>(request.variant);
  std::string payload;
  payload.reserve(kMaxBaseNpAdvSize);
  payload.push_back(kBaseVersion);
  absl::Status result;
  std::string tx_power = {static_cast<char>(request.tx_power)};
  std::string action = SerializeAction(presence.action);
  uint8_t identity_type =
      GetIdentityFieldType(presence.credential_selector.identity_type);
  bool needs_encryption =
      identity_type != DataElement::kPublicIdentityFieldType;
  if (needs_encryption) {
    if (request.salt.size() != kSaltSize) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Unsupported salt size %d", request.salt.size()));
    }
    if (!credential) {
      return absl::FailedPreconditionError("Missing credentials");
    }
    std::string unencrypted;
    result = AppendDataElement(DataElement::kTxPowerFieldType, tx_power,
                               unencrypted);
    if (!result.ok()) {
      return result;
    }
    result =
        AppendDataElement(DataElement::kActionFieldType, action, unencrypted);
    if (!result.ok()) {
      return result;
    }
    NEARBY_VLOG(1) << "Unencrypted advertisement payload "
                   << absl::BytesToHexString(unencrypted);
    absl::StatusOr<std::string> encrypted =
        EncryptDataElements(*credential, request.salt, unencrypted);
    if (!encrypted.ok()) {
      return encrypted.status();
    }
    if (encrypted->size() <= kBaseMetadataSize) {
      return absl::OutOfRangeError(
          absl::StrFormat("Encrypted identity DE is too short - %d bytes. "
                          "Expected more than %d",
                          encrypted->size(), kBaseMetadataSize));
    }

    // The Identity DE header does not include the length of salt nor metadata.
    absl::StatusOr<uint8_t> identity_header = CreateDataElementHeader(
        encrypted->size() - kBaseMetadataSize, identity_type);
    if (!identity_header.ok()) {
      return identity_header.status();
    }
    payload.push_back(*identity_header);
    // In the encrypted format, salt is not a DE (thus no header)
    payload.append(request.salt);
    payload.append(*encrypted);
  } else {
    result = AppendDataElement(identity_type, "", payload);
    if (!result.ok()) {
      return result;
    }
    if (!request.salt.empty()) {
      result =
          AppendDataElement(DataElement::kSaltFieldType, request.salt, payload);
      if (!result.ok()) {
        return result;
      }
    }
    result =
        AppendDataElement(DataElement::kTxPowerFieldType, tx_power, payload);
    if (!result.ok()) {
      return result;
    }
    result = AppendDataElement(DataElement::kActionFieldType, action, payload);
    if (!result.ok()) {
      return result;
    }
  }
  return AdvertisementData{.is_extended_advertisement = false,
                           .content = payload};
}
absl::StatusOr<std::string> AdvertisementFactory::EncryptDataElements(
    const LocalCredential& credential, absl::string_view salt,
    absl::string_view data_elements) const {
  if (credential.metadata_encryption_key_v0().size() != kBaseMetadataSize) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Metadata key size %d, expected %d",
        credential.metadata_encryption_key_v0().size(), kBaseMetadataSize));
  }

  // HMAC is not used during encryption, so we can pass an empty value.
  absl::StatusOr<LdtEncryptor> encryptor =
      LdtEncryptor::Create(credential.key_seed(), /*known_hmac=*/"");
  if (!encryptor.ok()) {
    return encryptor.status();
  }
  std::string plaintext =
      absl::StrCat(credential.metadata_encryption_key_v0(), data_elements);
  return encryptor->Encrypt(plaintext, salt);
}

absl::StatusOr<CredentialSelector> AdvertisementFactory::GetCredentialSelector(
    const BaseBroadcastRequest& request) {
  if (absl::holds_alternative<BaseBroadcastRequest::BasePresence>(
          request.variant)) {
    const auto& presence =
        absl::get<BaseBroadcastRequest::BasePresence>(request.variant);
    if (RequiresCredentials(presence.credential_selector.identity_type)) {
      return presence.credential_selector;
    }
  }
  return absl::NotFoundError("credentials not required");
}
}  // namespace presence
}  // namespace nearby
