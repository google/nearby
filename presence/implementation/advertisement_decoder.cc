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

namespace nearby {
namespace presence {

constexpr uint8_t kDataTypeMask =
    (1 << DataElement::kDataElementLengthShift) - 1;

constexpr int kAdvertisementVersion = 0;

namespace {

absl::StatusOr<DataElement> ParseDataElement(const absl::string_view input,
                                             size_t& index) {
  if (index >= input.size()) {
    return absl::OutOfRangeError(absl::StrFormat(
        "Data element (%s) is %d bytes long. Expected more than %d",
        absl::BytesToHexString(input), input.size(), index));
  }
  uint8_t header = input[index];
  uint8_t data_type = header & kDataTypeMask;
  size_t length = header >> DataElement::kDataElementLengthShift;
  index++;
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

bool IsIdentity(int data_type) {
  return data_type >= DataElement::kPrivateIdentityFieldType &&
         data_type <= DataElement::kProvisionedIdentityFieldType;
}

void DecodeBaseAction(absl::string_view serialized_action,
                      std::vector<DataElement>& output) {
  if (serialized_action.size() != sizeof(uint16_t)) {
    NEARBY_LOGS(WARNING) << "Base NP action \'"
                         << absl::BytesToHexString(serialized_action)
                         << "\' has wrong length " << serialized_action.size()
                         << " , expected " << sizeof(uint16_t);
    return;
  }
  // Two bytes in Big Endian order
  uint8_t high = serialized_action[0];
  uint8_t low = serialized_action[1];
  Action action = {.action = static_cast<uint16_t>((high << 8) | low)};

  ActionFactory::DecodeAction(action, output);
}

}  // namespace

absl::StatusOr<std::vector<DataElement>>
AdvertisementDecoder::DecodeAdvertisement(absl::string_view advertisement) {
  std::vector<DataElement> result;
  NEARBY_LOGS(VERBOSE) << "Advertisement: "
                       << absl::BytesToHexString(advertisement);
  if (advertisement.empty()) {
    return absl::OutOfRangeError("Empty advertisement");
  }
  int version = advertisement[0];
  NEARBY_LOGS(VERBOSE) << "Version: " << version;
  if (version != kAdvertisementVersion) {
    return absl::UnimplementedError(
        absl::StrFormat("Advertisment version (%d) is not supported", version));
  }
  size_t index = 1;
  std::string salt;
  absl::StatusOr<std::string> decrypted;
  while (index < advertisement.size()) {
    absl::StatusOr<DataElement> elem = ParseDataElement(advertisement, index);
    if (!elem.ok()) {
      NEARBY_LOGS(WARNING) << "Failed to read data element, status: "
                           << elem.status();
      return elem.status();
    }
    if (elem->GetType() == DataElement::kSaltFieldType) {
      salt = elem->GetValue();
    }
    bool need_decryption =
        IsIdentity(elem->GetType()) && !elem->GetValue().empty();
    if (need_decryption && index < advertisement.size()) {
      NEARBY_LOGS(VERBOSE) << "Metadata: "
                           << absl::BytesToHexString(elem->GetValue())
                           << "Salt: " << absl::BytesToHexString(salt)
                           << "Encrypted: "
                           << absl::BytesToHexString(
                                  advertisement.substr(index));
      decrypted = credential_manager_.DecryptDataElements(
          elem->GetValue(), salt, advertisement.substr(index));
      if (!decrypted.ok()) {
        NEARBY_LOGS(WARNING) << "Failed to decrypt advertisement, status: "
                             << decrypted.status();
        return decrypted.status();
      }
      // Restart the loop and iterate over the decrypted content
      advertisement = *decrypted;
      index = 0;
    }
    if (elem->GetType() == DataElement::kActionFieldType) {
      DecodeBaseAction(elem->GetValue(), result);
    } else {
      result.push_back(*std::move(elem));
    }
  }
  return result;
}

}  // namespace presence
}  // namespace nearby
