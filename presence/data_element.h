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

#ifndef THIRD_PARTY_NEARBY_PRESENCE_DATA_ELEMENT_H_
#define THIRD_PARTY_NEARBY_PRESENCE_DATA_ELEMENT_H_

#include <stdint.h>

#include <initializer_list>
#include <ostream>
#include <string>

#include "absl/strings/escaping.h"
#include "absl/strings/string_view.h"
namespace nearby {
namespace presence {

// Reserved Action types when the field type is kActionFieldType.
// The values are bit numbers in BE ordering.
// TODO(b/338107166): these are out of date, need to be updated to latest spec
enum class ActionBit {
  kCallTransferAction = 4,
  kActiveUnlockAction = 8,
  kNearbyShareAction = 9,
  kInstantTetheringAction = 10,
  kPhoneHubAction = 11,
  kPresenceManagerAction = 12,
  kFinderAction = 13,
  kFastPairSassAction = 14,
  kTapToTransferAction = 15,
  kLastAction
};

// helpful for enumerating overall all possible action bit types, this must be
// kept in sync with the above enum
constexpr std::initializer_list<ActionBit> kAllActionBits = {
    ActionBit::kCallTransferAction, ActionBit::kActiveUnlockAction,
    ActionBit::kNearbyShareAction,  ActionBit::kInstantTetheringAction,
    ActionBit::kPhoneHubAction,     ActionBit::kPresenceManagerAction,
    ActionBit::kFinderAction,       ActionBit::kFastPairSassAction,
    ActionBit::kTapToTransferAction};

/** Describes a custom Data element in NP advertisement. */
class DataElement {
 public:
  // The field types listed below require special processing when generating and
  // parsing NP advertisements.
  static constexpr int kSaltFieldType = 0;
  static constexpr int kPrivateGroupIdentityFieldType = 1;
  static constexpr int kContactsGroupIdentityFieldType = 2;
  static constexpr int kPublicIdentityFieldType = 3;
  static constexpr int kTxPowerFieldType = 5;
  static constexpr int kActionFieldType = 6;
  static constexpr int kModelIdFieldType = 7;
  static constexpr int kEddystoneIdFieldType = 8;
  static constexpr int kAccountKeyDataFieldType = 9;
  static constexpr int kConnectionStatusFieldType = 10;
  static constexpr int kBatteryFieldType = 11;
  static constexpr int kAdvertisementSignature = 12;
  static constexpr int kContextTimestampFieldType = 13;
  // Maximum allowed Data Element's value length
  static constexpr int kMaxDataElementLength = 15;
  // Maximum allowed Data Element's type
  static constexpr int kMaxDataElementType = 15;
  // The DE header is (length << kDataElementLengthShift | type)
  static constexpr int kDataElementLengthShift = 4;

  DataElement(uint16_t type, absl::string_view value)
      : type_(type), value_(value) {}

  DataElement(uint16_t type, uint8_t value)
      : type_(type),
        value_(reinterpret_cast<const char*>(&value), sizeof(value)) {}

  explicit DataElement(ActionBit action)
      : DataElement(kActionFieldType, static_cast<uint8_t>(action)) {}

  ~DataElement() = default;

  uint16_t GetType() const { return type_; }
  absl::string_view GetValue() const { return value_; }

 private:
  uint16_t type_;
  std::string value_;
};

inline bool operator==(const DataElement& i1, const DataElement& i2) {
  return i1.GetType() == i2.GetType() && i1.GetValue() == i2.GetValue();
}

inline std::ostream& operator<<(std::ostream& os, const DataElement& elem) {
  return os << "DataElement(" << elem.GetType() << ", "
            << absl::BytesToHexString(elem.GetValue()) << ")";
}

}  // namespace presence
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_PRESENCE_DATA_ELEMENT_H_
