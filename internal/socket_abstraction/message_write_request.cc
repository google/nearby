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

#include "internal/socket_abstraction/message_write_request.h"

#include <algorithm>
#include <string>

#include "absl/status/status.h"

namespace nearby {
namespace socket_abstraction {

MessageWriteRequest::MessageWriteRequest(ByteArray message) {
  message_ = message;
  position_ = 0;
}

bool MessageWriteRequest::IsStarted() { return position_ != 0; }

bool MessageWriteRequest::IsFinished() { return position_ == message_.size(); }

absl::StatusOr<Packet> MessageWriteRequest::NextPacket(int max_packet_size) {
  if (IsFinished()) {
    return absl::ResourceExhaustedError(
        "Message is finished sending, no more packets.");
  }
  bool isFirst = position_ == 0;
  int length = std::min(max_packet_size - 1, (int)message_.size() - position_);
  std::string subseq =
      std::string(message_.AsStringView().substr(position_, length));
  position_ += length;
  bool isLast = position_ == message_.size();
  return Packet::CreateDataPacket(isFirst, isLast, ByteArray(subseq));
}

}  // namespace socket_abstraction
}  // namespace nearby
