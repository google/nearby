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
#include "presence/data_element.h"
#include "presence/implementation/action_factory.h"
#include "presence/implementation/base_broadcast_request.h"

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
    case DataElement::kActionFieldType:
      // In v0 OTA format, this is a combined TX and Action DE.
      return length >= 1 && length <= 5;
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

void DecodeBaseTxAndAction(absl::string_view serialized_action,
                           std::vector<DataElement>& output) {
  if (serialized_action.size() < sizeof(uint8_t) ||
      serialized_action.size() > sizeof(uint8_t) + sizeof(uint32_t)) {
    NEARBY_LOGS(WARNING) << "Base NP action \'"
                         << absl::BytesToHexString(serialized_action)
                         << "\' has wrong length " << serialized_action.size()
                         << " , expected size in range [1 - 5]";
    return;
  }
  // TX power.
  uint8_t tx_power = serialized_action[0];
  output.emplace_back(DataElement::kTxPowerFieldType, tx_power);

  // Action, 0-4 bytes in Big Endian order.
  Action action = {.action = 0};
  constexpr int kActionOffset = sizeof(uint8_t);
  for (int i = 0; i < serialized_action.size() - kActionOffset; ++i) {
    int offset = (sizeof(uint32_t) - 1 - i) * 8;
    action.action |= serialized_action[i + kActionOffset] << offset;
  }

  ActionFactory::DecodeAction(action, output);
}

}  // namespace

absl::Status AdvertisementDecoder::DecryptDataElements(
    const DataElement& elem, std::vector<DataElement>& result) {
  if (elem.GetValue().size() <= kEncryptedIdentityAdditionalLength) {
    return absl::OutOfRangeError(absl::StrFormat(
        "Encrypted identity data element is too short - %d bytes",
        elem.GetValue().size()));
  }
  absl::string_view salt = elem.GetValue().substr(0, kSaltSize);
  result.emplace_back(DataElement::kSaltFieldType, salt);
  absl::string_view encrypted = elem.GetValue().substr(kSaltSize);
  absl::StatusOr<std::string> decrypted =
      credential_manager_.DecryptDataElements(salt, encrypted);
  if (!decrypted.ok()) {
    NEARBY_LOGS(WARNING) << "Failed to decrypt advertisement, status: "
                         << decrypted.status();
    return decrypted.status();
  }
  if (decrypted->size() <= kBaseMetadataSize) {
    return absl::OutOfRangeError(absl::StrFormat(
        "Decrypted identity DE is too short - %d bytes. Expected more than %d",
        decrypted->size(), kBaseMetadataSize));
  }
  result.emplace_back(elem.GetType(), decrypted->substr(0, kBaseMetadataSize));
  size_t index = kBaseMetadataSize;
  while (index < decrypted->size()) {
    absl::StatusOr<DataElement> internal_elem =
        ParseDataElement(*decrypted, index);
    if (!internal_elem.ok()) {
      NEARBY_LOGS(WARNING) << "Failed to read data element, status: "
                           << internal_elem.status();
      return internal_elem.status();
    }
    if (internal_elem->GetType() == DataElement::kActionFieldType) {
      // In v0 OTA format, this is a combined TX and Action DE.
      DecodeBaseTxAndAction(internal_elem->GetValue(), result);
    } else {
      result.push_back(*std::move(internal_elem));
    }
  }
  return absl::OkStatus();
}

absl::StatusOr<std::vector<DataElement>>
AdvertisementDecoder::DecodeAdvertisement(absl::string_view advertisement) {
  std::vector<DataElement> result;
  NEARBY_LOGS(VERBOSE) << "Advertisement: "
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
  size_t index = 1;
  absl::StatusOr<std::string> decrypted;
  while (index < advertisement.size()) {
    absl::StatusOr<DataElement> elem = ParseDataElement(advertisement, index);
    if (!elem.ok()) {
      NEARBY_LOGS(WARNING) << "Failed to read data element, status: "
                           << elem.status();
      return elem.status();
    }
    if (IsEncryptedIdentity(elem->GetType())) {
      absl::Status status = DecryptDataElements(*elem, result);
      if (!status.ok()) {
        return status;
      }
    } else {
      if (elem->GetType() == DataElement::kActionFieldType) {
        // In v0 OTA format, this is a combined TX and Action DE.
        DecodeBaseTxAndAction(elem->GetValue(), result);
      } else {
        result.push_back(*std::move(elem));
      }
    }
  }
  return result;
}

}  // namespace presence
}  // namespace nearby
