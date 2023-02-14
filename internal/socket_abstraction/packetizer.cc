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

#include "internal/socket_abstraction/packetizer.h"

#include "absl/status/status.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/mutex_lock.h"
#include "internal/socket_abstraction/packet.h"

namespace nearby {
namespace socket_abstraction {

absl::StatusOr<ByteArray> Packetizer::Join(Packet packet) {
  MutexLock lock(&mutex_);
  absl::StatusOr<ByteArray> result;
  if (pending_payloads_.empty() && !packet.IsFirstPacket()) {
    return absl::UnavailableError(
        "First packet must be the first packet added.");
  }
  if (!pending_payloads_.empty() && packet.IsFirstPacket()) {
    return absl::InvalidArgumentError(
        "First packet cannot be added if there are pending packets.");
  }
  ByteArray copy = ByteArray(packet.GetPayload());
  pending_payloads_.push_back(copy);
  pending_payloads_size_ += copy.size();
  if (packet.IsLastPacket()) {
    ByteArray message = ByteArray(pending_payloads_size_);
    int offset = 0;
    for (const auto& elem : pending_payloads_) {
      message.CopyAt(offset, elem);
      offset += elem.size();
    }
    pending_payloads_.clear();
    pending_payloads_size_ = 0;
    result = message;
  } else {
    result = absl::UnavailableError("No last packet found yet.");
  }
  return result;
}

void Packetizer::Reset() {
  MutexLock lock(&mutex_);
  pending_payloads_.clear();
  pending_payloads_size_ = 0;
}

}  // namespace socket_abstraction
}  // namespace nearby
