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

#include "presence/implementation/advertisement_decoder.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "internal/platform/logging.h"
#include "internal/proto/credential.pb.h"
#include "presence/data_element.h"
#include "presence/implementation/action_factory.h"
#include "presence/implementation/base_broadcast_request.h"
#include "presence/implementation/ldt.h"

namespace nearby {
namespace presence {

namespace {

constexpr uint8_t kDataTypeMask =
    (1 << DataElement::kDataElementLengthShift) - 1;

constexpr int kAdvertisementVersion = 0;

constexpr int kEncryptedIdentityAdditionalLength =
    kSaltSize + kBaseMetadataSize;
constexpr int kEddystoneAdditionalLength = 20;

uint8_t GetDataElementType(uint8_t header) { return header & kDataTypeMask; }

size_t GetDataElementLength(uint8_t header) {
  return header >> DataElement::kDataElementLengthShift;
}

// Verifies if the DE header describes a valid DE in v0 advertisement.
bool IsDataElementAllowed(uint8_t header) {
  uint8_t data_type = GetDataElementType(header);
  size_t length = GetDataElementLength(header);
  switch (data_type) {
    case DataElement::kSaltFieldType:
      return length == 2;
    case DataElement::kPublicIdentityFieldType:
      return length == 0;
    case DataElement::kPrivateIdentityFieldType:
    case DataElement::kProvisionedIdentityFieldType:
    case DataElement::kTrustedIdentityFieldType:
      return length >= 2 && length <= 6;
    case DataElement::kTxPowerFieldType:
      return length == 1;
    case DataElement::kActionFieldType:
      return length >= 1 && length <= 3;
    case DataElement::kModelIdFieldType:
      return length == 3;
    case DataElement::kEddystoneIdFieldType:
      return length == 0;
    case DataElement::kAccountKeyDataFieldType:
      return length <= 12;
    case DataElement::kConnectionStatusFieldType:
      return length <= 3;
    case DataElement::kBatteryFieldType:
      return length <= 3;
    default:
      return false;
  }
}

bool IsEncryptedIdentity(int data_type) {
  return data_type == DataElement::kPrivateIdentityFieldType ||
         data_type == DataElement::kTrustedIdentityFieldType ||
         data_type == DataElement::kProvisionedIdentityFieldType;
}

bool IsIdentity(int data_type) {
  return data_type == DataElement::kPublicIdentityFieldType ||
         IsEncryptedIdentity(data_type);
}

internal::IdentityType GetIdentityType(int data_type) {
  switch (data_type) {
    case DataElement::kPrivateIdentityFieldType:
      return internal::IDENTITY_TYPE_PRIVATE;
    case DataElement::kTrustedIdentityFieldType:
      return internal::IDENTITY_TYPE_TRUSTED;
    case DataElement::kProvisionedIdentityFieldType:
      return internal::IDENTITY_TYPE_PROVISIONED;
    case DataElement::kPublicIdentityFieldType:
      return internal::IDENTITY_TYPE_PUBLIC;
  }
  return internal::IDENTITY_TYPE_UNSPECIFIED;
}

// Returns the real length of a DE in v0 advertisement, which may be larger than
// the value in the header.
size_t GetDataElementTrueLength(uint8_t header) {
  uint8_t data_type = GetDataElementType(header);
  size_t length = GetDataElementLength(header);
  if (IsEncryptedIdentity(data_type)) {
    // The length of an encrypted DE is 16 bytes of overhead (salt + metadata
    // key) + the actual payload, which is too long to fit in the 4-bit DE
    // length field.
    length += kEncryptedIdentityAdditionalLength;
  } else if (data_type == DataElement::kEddystoneIdFieldType) {
    // Length in the header is set to EID length minus 20, because EID is longer
    // than 15 (the maximum length that can be stored in 4 bits.
    length += kEddystoneAdditionalLength;
  }
  return length;
}

absl::StatusOr<DataElement> ParseDataElement(const absl::string_view input,
                                             size_t& index) {
  if (index >= input.size()) {
    return absl::OutOfRangeError(absl::StrFormat(
        "Data element (%s) is %d bytes long. Expected more than %d",
        absl::BytesToHexString(input), input.size(), index));
  }
  uint8_t header = input[index];
  if (!IsDataElementAllowed(header)) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Unsupported Data Element 0x%x", header));
  }
  uint8_t data_type = GetDataElementType(header);
  size_t length = GetDataElementTrueLength(header);
  ++index;
  size_t start = index;
  index += length;
  if (index > input.size()) {
    return absl::OutOfRangeError(absl::StrFormat(
        "Data element (%s) is %d bytes long. Expected at least %d",
        absl::BytesToHexString(input), input.size(), index));
  }
  NEARBY_LOGS(VERBOSE) << "Type: " << static_cast<int>(data_type)
                       << " length: " << static_cast<int>(length) << " DE: "
                       << absl::BytesToHexString(input.substr(start, length));
  return DataElement(data_type, input.substr(start, length));
}

bool Contains(const std::vector<DataElement>& data_elements,
              const DataElement& data_element) {
  return std::find(data_elements.begin(), data_elements.end(), data_element) !=
         data_elements.end();
}

bool ContainsAll(const std::vector<DataElement>& data_elements,
                 const std::vector<DataElement>& extended_properties) {
  for (const auto& filter_element : extended_properties) {
    if (!Contains(data_elements, filter_element)) {
      return false;
    }
  }
  return true;
}

bool ContainsAny(const std::vector<DataElement>& data_elements,
                 const std::vector<int>& actions) {
  if (actions.empty()) {
    return true;
  }
  for (int action : actions) {
    if (Contains(data_elements, DataElement(ActionBit(action)))) {
      return true;
    }
  }
  return false;
}

}  // namespace

void AdvertisementDecoder::DecodeBaseAction(
    absl::string_view serialized_action) {
  if (serialized_action.empty() || serialized_action.size() > 3) {
    NEARBY_LOGS(WARNING) << "Base NP action \'"
                         << absl::BytesToHexString(serialized_action)
                         << "\' has wrong length " << serialized_action.size()
                         << " , expected size in range [1 - 3]";
    return;
  }
  // Action, 0-2 bytes in Big Endian order.
  Action action = {.action = 0};
  for (int i = 0; i < serialized_action.size(); ++i) {
    int offset = (sizeof(uint32_t) - 1 - i) * 8;
    action.action |= serialized_action[i] << offset;
  }

  ActionFactory::DecodeAction(action, decoded_advertisement_.data_elements);
}

absl::StatusOr<std::string> AdvertisementDecoder::DecryptLdt(
    const std::vector<internal::SharedCredential>& credentials,
    absl::string_view salt, absl::string_view data_elements) {
  if (credentials.empty()) {
    return absl::UnavailableError("No credentials");
  }
  for (const auto& credential : credentials) {
    absl::StatusOr<LdtEncryptor> encryptor = LdtEncryptor::Create(
        credential.key_seed(),
        credential.metadata_encryption_key_tag_v0());
    if (encryptor.ok()) {
      absl::StatusOr<std::string> result =
          encryptor->DecryptAndVerify(data_elements, salt);
      if (result.ok() && result->size() > kBaseMetadataSize) {
        decoded_advertisement_.public_credential = credential;
        decoded_advertisement_.metadata_key =
            result->substr(0, kBaseMetadataSize);
        return result->substr(kBaseMetadataSize);
      }
    }
  }
  return absl::UnavailableError(
      "Couldn't decrypt the message with any credentials");
}

absl::Status AdvertisementDecoder::DecryptDataElements(
    const DataElement& elem) {
  if (elem.GetValue().size() <= kEncryptedIdentityAdditionalLength) {
    return absl::OutOfRangeError(absl::StrFormat(
        "Encrypted identity data element is too short - %d bytes",
        elem.GetValue().size()));
  }
  absl::string_view salt = elem.GetValue().substr(0, kSaltSize);
  decoded_advertisement_.data_elements.emplace_back(DataElement::kSaltFieldType,
                                                    salt);
  absl::string_view encrypted = elem.GetValue().substr(kSaltSize);
  absl::StatusOr<std::string> decrypted = Decrypt(salt, encrypted);
  if (!decrypted.ok()) {
    NEARBY_LOGS(WARNING) << "Failed to decrypt advertisement, status: "
                         << decrypted.status();
    return decrypted.status();
  }
  size_t index = 0;
  while (index < decrypted->size()) {
    absl::StatusOr<DataElement> internal_elem =
        ParseDataElement(*decrypted, index);
    if (!internal_elem.ok()) {
      NEARBY_LOGS(WARNING) << "Failed to read data element, status: "
                           << internal_elem.status();
      return internal_elem.status();
    }
    if (internal_elem->GetType() == DataElement::kActionFieldType) {
      DecodeBaseAction(internal_elem->GetValue());
    } else {
      decoded_advertisement_.data_elements.push_back(*std::move(internal_elem));
    }
  }
  return absl::OkStatus();
}

absl::StatusOr<std::string> AdvertisementDecoder::Decrypt(
    absl::string_view salt, absl::string_view encrypted) {
  for (const auto& scan_filter : scan_request_.scan_filters) {
    if (!absl::holds_alternative<LegacyPresenceScanFilter>(scan_filter)) {
      continue;
    }
    const std::vector<nearby::internal::SharedCredential>& credentials =
        absl::get<LegacyPresenceScanFilter>(scan_filter)
            .remote_public_credentials;
    if (credentials.empty()) {
      continue;
    }
    absl::StatusOr<std::string> decrypted =
        DecryptLdt(credentials, salt, encrypted);
    if (decrypted.ok()) {
      return decrypted;
    }
  }
  if (credentials_ == nullptr) {
    return absl::FailedPreconditionError("Missing credentials");
  }

  return DecryptLdt((*credentials_)[decoded_advertisement_.identity_type], salt,
                    encrypted);
}

void AdvertisementDecoder::AddBannedDataTypes() {
  // The scan request has information what identity types the client is
  // interested in. We'll ban all other idenitity data types.
  banned_data_types_ = {DataElement::kPrivateIdentityFieldType,
                        DataElement::kTrustedIdentityFieldType,
                        DataElement::kPublicIdentityFieldType,
                        DataElement::kProvisionedIdentityFieldType};
  for (nearby::internal::IdentityType identity_type :
       scan_request_.identity_types) {
    switch (identity_type) {
      case internal::IDENTITY_TYPE_PRIVATE:
        banned_data_types_.erase(DataElement::kPrivateIdentityFieldType);
        break;
      case internal::IDENTITY_TYPE_TRUSTED:
        banned_data_types_.erase(DataElement::kTrustedIdentityFieldType);
        break;
      case internal::IDENTITY_TYPE_PUBLIC:
        banned_data_types_.erase(DataElement::kPublicIdentityFieldType);
        break;
      case internal::IDENTITY_TYPE_PROVISIONED:
        banned_data_types_.erase(DataElement::kProvisionedIdentityFieldType);
        break;
      default:
        // Nothing to do
        break;
    }
  }
}

absl::StatusOr<Advertisement> AdvertisementDecoder::DecodeAdvertisement(
    absl::string_view advertisement) {
  // Let's keep the result advertisement in a member variable to avoid passing
  // it around all the time.
  decoded_advertisement_ = Advertisement{};
  std::vector<DataElement> result;
  NEARBY_LOGS(INFO) << "Advertisement: "
                    << absl::BytesToHexString(advertisement);
  if (advertisement.empty()) {
    return absl::OutOfRangeError("Empty advertisement");
  }
  uint8_t version = advertisement[0];
  NEARBY_LOGS(VERBOSE) << "Version: " << version;
  if (version != kAdvertisementVersion) {
    return absl::UnimplementedError(absl::StrFormat(
        "Advertisement version (%d) is not supported", version));
  }
  decoded_advertisement_.version = version;
  size_t index = 1;
  absl::StatusOr<std::string> decrypted;
  while (index < advertisement.size()) {
    absl::StatusOr<DataElement> elem = ParseDataElement(advertisement, index);
    if (!elem.ok()) {
      NEARBY_LOGS(WARNING) << "Failed to read data element, status: "
                           << elem.status();
      return elem.status();
    }
    // This checks allows us to bail before decryption when, for example, the
    // client is scanning for advertisements with private identity but the
    // advertisement uses trusted identity.
    if (banned_data_types_.contains(elem->GetType())) {
      return absl::FailedPreconditionError(
          absl::StrFormat("Ignoring advertisement with data element type: %d",
                          elem->GetType()));
    }
    if (IsIdentity(elem->GetType())) {
      decoded_advertisement_.identity_type = GetIdentityType(elem->GetType());
    }
    if (IsEncryptedIdentity(elem->GetType())) {
      absl::Status status = DecryptDataElements(*elem);
      if (!status.ok()) {
        return status;
      }
    } else {
      if (elem->GetType() == DataElement::kActionFieldType) {
        DecodeBaseAction(elem->GetValue());
      } else {
        decoded_advertisement_.data_elements.push_back(*std::move(elem));
      }
    }
  }
  return std::move(decoded_advertisement_);
}

bool AdvertisementDecoder::MatchesScanFilter(
    const std::vector<DataElement>& data_elements) {
  // The advertisement matches the scan request when it matches at least
  // one of the filters in the request.
  if (scan_request_.scan_filters.empty()) {
    return true;
  }
  for (const auto& filter : scan_request_.scan_filters) {
    if (absl::holds_alternative<PresenceScanFilter>(filter)) {
      if (MatchesScanFilter(data_elements,
                            absl::get<PresenceScanFilter>(filter))) {
        return true;
      }
    } else if (absl::holds_alternative<LegacyPresenceScanFilter>(filter)) {
      if (MatchesScanFilter(data_elements,
                            absl::get<LegacyPresenceScanFilter>(filter))) {
        return true;
      }
    }
  }
  return false;
}

bool AdvertisementDecoder::MatchesScanFilter(
    const std::vector<DataElement>& data_elements,
    const PresenceScanFilter& filter) {
  // The advertisement must contain all Data Elements in scan request.
  return ContainsAll(data_elements, filter.extended_properties);
}

bool AdvertisementDecoder::MatchesScanFilter(
    const std::vector<DataElement>& data_elements,
    const LegacyPresenceScanFilter& filter) {
  // The advertisement must:
  // * contain any Action from scan request,
  // * contain all Data Elements in scan request.
  return ContainsAny(data_elements, filter.actions) &&
         ContainsAll(data_elements, filter.extended_properties);
}

std::vector<CredentialSelector> AdvertisementDecoder::GetCredentialSelectors(
    const ScanRequest& scan_request) {
  std::vector<IdentityType> all_types = {
      IdentityType::IDENTITY_TYPE_PRIVATE, IdentityType::IDENTITY_TYPE_TRUSTED,
      IdentityType::IDENTITY_TYPE_PUBLIC,
      IdentityType::IDENTITY_TYPE_PROVISIONED};
  std::vector<CredentialSelector> selectors;
  for (auto identity_type :
       (scan_request.identity_types.empty() ? all_types
                                            : scan_request.identity_types)) {
    selectors.push_back(
        CredentialSelector{.manager_app_id = scan_request.manager_app_id,
                           .account_name = scan_request.account_name,
                           .identity_type = identity_type});
  }
  return selectors;
}

}  // namespace presence
}  // namespace nearby
