// Copyright 2023 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_MESSAGE_STREAM_MESSAGE_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_MESSAGE_STREAM_MESSAGE_H_

#include <stdint.h>

#include <ostream>
#include <string>

#include "absl/strings/escaping.h"
#include "internal/platform/byte_array.h"

namespace nearby {
namespace fastpair {

enum class MessageGroup {
  kBluetooth = 1,
  kCompanionAppEvent = 2,
  kDeviceInformationEvent = 3,
  kDeviceActionEvent = 4,
  kSass = 7,
  kAcknowledgement = 255
};

// Note, message code values are not unique, because every message group has
// their own list of message codes.
enum class MessageCode {
  // Message codes for kBluetooth message group
  kEnableSilenceMode = 1,
  kDisableSilenceMode = 2,
  // Message codes for kCompanionAppEvent message group
  kLogBufferFull = 1,
  // Message codes for kDeviceInformationEvent message group
  kModelId = 1,
  kBleAddressUpdated = 2,
  kBatteryUpdated = 3,
  kRemainingBatteryTime = 4,
  kActiveComponentRequest = 5,
  kActiveComponentResponse = 6,
  kCapabilites = 7,
  kPlatformType = 8,
  kSessionNonce = 0x0A,
  // Message codes for kDeviceActionEvent message group
  kRing = 1,
  // Message codes for kSass message group
  kSassGetCapability = 0x10,
  kSassNotifyCapability = 0x11,
  kSassSetMultipointState = 0x12,
  kSassSetSwitchingPreference = 0x20,
  kSassGetSwitchingPreference = 0x21,
  kSassNotifySwitchingPreference = 0x22,
  kSassSwitchActiveSourceCode = 0x30,
  kSassSwitchBackAudioSource = 0x31,
  kSassNotifyMultipointSwitchEvent = 0x32,
  kSassGetConnectionStatus = 0x33,
  kSassNotifyConnectionStatus = 0x34,
  kSassNotifySassInitiatedConnection = 0x40,
  kSassInUseAccountKey = 0x41,
  kSassSendCustomData = 0x42,
  kSassSetDropConnectionTarget = 0x43,
  // Message codes for kAcknowledgement message group
  kAck = 1,
  kNack = 2
};

struct Message {
  MessageGroup message_group;
  MessageCode message_code;
  std::string payload;
};

inline std::ostream& operator<<(std::ostream& os, const Message& message) {
  os << "Message{" << static_cast<int>(message.message_group) << ", "
     << static_cast<int>(message.message_code);
  if (!message.payload.empty()) {
    os << ", '" << absl::BytesToHexString(message.payload) << "'";
  }
  os << "}";

  return os;
}

inline bool operator==(const Message& a, const Message& b) {
  return a.message_group == b.message_group &&
         a.message_code == b.message_code && a.payload == b.payload;
}
}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_MESSAGE_STREAM_MESSAGE_H_
