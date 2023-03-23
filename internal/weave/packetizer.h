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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_WEAVE_PACKETIZER_H_
#define THIRD_PARTY_NEARBY_INTERNAL_WEAVE_PACKETIZER_H_

#include <string>

#include "absl/status/statusor.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/mutex.h"
#include "internal/weave/packet.h"

namespace nearby {
namespace weave {

// Joins Weave packets to create messages.
class Packetizer {
 public:
  // Adds a Packet to an ongoing message, returning absl::OkStatus() on success.
  //
  // Returns absl::InvalidArgumentError if the packet being added is
  // not marked as the first packet (if no prior packets have been joined) or if
  // the packet being joined is marked as the first packet, but there is already
  // a message being reconstructed.
  absl::Status AddPacket(Packet packet) ABSL_LOCKS_EXCLUDED(mutex_);

  // Should be called after each call to AddPacket() to get the message if
  // available. This function will return absl::UnavailableError should the
  // message not be available at the time of calling. This will clear the
  // packetizer buffer on a successful call.
  absl::StatusOr<ByteArray> TakeMessage() ABSL_LOCKS_EXCLUDED(mutex_);

  // Resets the packetizer, clearing any pending messages.
  void Reset() ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  Mutex mutex_;
  std::string pending_payloads_ ABSL_GUARDED_BY(mutex_);
  bool is_message_complete_ ABSL_GUARDED_BY(mutex_) = false;
};
}  // namespace weave
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_WEAVE_PACKETIZER_H_
