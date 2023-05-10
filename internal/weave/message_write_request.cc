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

#include "internal/weave/message_write_request.h"

#include <algorithm>
#include <string>

#include "absl/status/status.h"

namespace nearby {
namespace weave {

MessageWriteRequest::MessageWriteRequest(absl::string_view message) {
  message_ = std::string(message);
  position_ = 0;
}

bool MessageWriteRequest::IsStarted() const { return position_ != 0; }

bool MessageWriteRequest::IsFinished() const {
  return position_ >= message_.size();
}

// max_packet_size is determined and checked at the socket level, so we only
// need to make sure we're getting a valid packet size > 0. We will take
// max_packet_size - 1 (due to the header) bytes from the message if available,
// otherwise taking the remaining bytes in the message and completing the
// request.
absl::StatusOr<Packet> MessageWriteRequest::NextPacket(int max_packet_size) {
  if (IsFinished()) {
    return absl::OutOfRangeError(
        "Message is finished sending, no more packets.");
  }
  if (max_packet_size <= 0) {
    return absl::InvalidArgumentError(
        "max_packet_size must be greater than 0.");
  }
  bool is_first = !IsStarted();
  int next_packet_len = std::min(max_packet_size - Packet::kPacketHeaderLength,
                                 (int)message_.size() - position_);
  auto next_packet_bytes = message_.substr(position_, next_packet_len);
  position_ += next_packet_len;
  return Packet::CreateDataPacket(is_first, IsFinished(),
                                  ByteArray(next_packet_bytes));
}

}  // namespace weave
}  // namespace nearby
