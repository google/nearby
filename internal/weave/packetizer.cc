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

#include "internal/weave/packetizer.h"

#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/mutex_lock.h"
#include "internal/weave/packet.h"

namespace nearby {
namespace weave {

absl::Status Packetizer::AddPacket(Packet packet) {
  MutexLock lock(&mutex_);
  if (is_message_complete_) {
    return absl::InvalidArgumentError(
        "Call GetMessage() first to retrieve message before adding another "
        "packet.");
  }
  if (pending_payloads_.empty() && !packet.IsFirstPacket()) {
    return absl::InvalidArgumentError(
        "First packet added must be marked as the first packet.");
  }
  if (!pending_payloads_.empty() && packet.IsFirstPacket()) {
    return absl::InvalidArgumentError(
        "Packet marked as first packet cannot be added if there are existing "
        "packets.");
  }
  absl::StrAppend(&pending_payloads_, packet.GetPayload());
  if (packet.IsLastPacket()) {
    is_message_complete_ = true;
  }
  return absl::OkStatus();
}

absl::StatusOr<ByteArray> Packetizer::TakeMessage() {
  MutexLock lock(&mutex_);
  if (!is_message_complete_) {
    return absl::UnavailableError(
        "Full message is not available, no last packet added yet.");
  }
  ByteArray message = ByteArray(std::move(pending_payloads_));
  pending_payloads_.clear();
  is_message_complete_ = false;
  return message;
}

void Packetizer::Reset() {
  MutexLock lock(&mutex_);
  pending_payloads_.clear();
  is_message_complete_ = false;
}

}  // namespace weave
}  // namespace nearby
