// Copyright 2020 Google LLC
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

#ifndef CORE_PAYLOAD_TYPE_H_
#define CORE_PAYLOAD_TYPE_H_

#include <ostream>

namespace nearby {
namespace connections {

enum class PayloadType { kUnknown = 0, kBytes = 1, kFile = 2, kStream = 3 };

enum class PayloadDirection {
  UNKNOWN_DIRECTION_PAYLOAD = 0,
  INCOMING_PAYLOAD = 1,
  OUTGOING_PAYLOAD = 2,
};

inline std::ostream& operator<<(std::ostream& os, PayloadType payload_type) {
  switch (payload_type) {
    case PayloadType::kUnknown:
      os << "kUnknown";
      break;
    case PayloadType::kBytes:
      os << "kBytes";
      break;
    case PayloadType::kFile:
      os << "kFile";
      break;
    case PayloadType::kStream:
      os << "kStream";
      break;
    default:
      os << "Invalid PayloadType";
      break;
  }
  return os;
}

inline std::ostream& operator<<(std::ostream& os,
                                PayloadDirection payload_direction) {
  switch (payload_direction) {
    case PayloadDirection::UNKNOWN_DIRECTION_PAYLOAD:
      os << "UNKNOWN_DIRECTION_PAYLOAD";
      break;
    case PayloadDirection::INCOMING_PAYLOAD:
      os << "INCOMING_PAYLOAD";
      break;
    case PayloadDirection::OUTGOING_PAYLOAD:
      os << "OUTGOING_PAYLOAD";
      break;
    default:
      os << "Invalid PayloadDirection";
      break;
  }
  return os;
}

}  // namespace connections
}  // namespace nearby

#endif  // CORE_PAYLOAD_TYPE_H_
