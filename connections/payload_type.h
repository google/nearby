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
#include "absl/strings/str_cat.h"

namespace nearby::connections {

enum class PayloadType { kUnknown = 0, kBytes = 1, kFile = 2, kStream = 3 };

// Support logging of PayloadType.
template <typename Sink>
void AbslStringify(Sink& sink, PayloadType payload_type) {
  switch (payload_type) {
    case PayloadType::kBytes:
      sink.Append("Bytes");
      break;
    case PayloadType::kStream:
      sink.Append("Stream");
      break;
    case PayloadType::kFile:
      sink.Append("File");
      break;
    case PayloadType::kUnknown:
      sink.Append("Unknown");
      break;
  }
}

inline std::ostream& operator<<(std::ostream& os, PayloadType payload_type) {
  return os << absl::StrCat(payload_type);
}

enum class PayloadDirection {
  UNKNOWN_DIRECTION_PAYLOAD = 0,
  INCOMING_PAYLOAD = 1,
  OUTGOING_PAYLOAD = 2,
};

// Support logging of PayloadDirection.
template <typename Sink>
void AbslStringify(Sink& sink, PayloadDirection payload_direction) {
  switch (payload_direction) {
    case PayloadDirection::UNKNOWN_DIRECTION_PAYLOAD:
      sink.Append("UNKNOWN_DIRECTION_PAYLOAD");
      break;
    case PayloadDirection::INCOMING_PAYLOAD:
      sink.Append("INCOMING_PAYLOAD");
      break;
    case PayloadDirection::OUTGOING_PAYLOAD:
      sink.Append("OUTGOING_PAYLOAD");
      break;
  }
}

inline std::ostream& operator<<(std::ostream& os,
                                  PayloadDirection payload_direction) {
  return os << absl::StrCat(payload_direction);
}

}  // namespace nearby::connections

#endif  // CORE_PAYLOAD_TYPE_H_
