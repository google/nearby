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

namespace nearby {
namespace connections {

enum class PayloadType { kUnknown = 0, kBytes = 1, kStream = 2, kFile = 3 };
enum PayloadDirection {
  UNKNOWN_DIRECTION_PAYLOAD = 0,
  INCOMING_PAYLOAD = 1,
  OUTGOING_PAYLOAD = 2,
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_PAYLOAD_TYPE_H_
