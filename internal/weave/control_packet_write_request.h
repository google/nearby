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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_WEAVE_SINGLE_PACKET_WRITE_REQUEST_H_
#define THIRD_PARTY_NEARBY_INTERNAL_WEAVE_SINGLE_PACKET_WRITE_REQUEST_H_

#include <utility>

#include "absl/status/status.h"
#include "internal/weave/packet.h"

namespace nearby {
namespace weave {

// This class encapsulates a Weave control packet to be sent over the wire.
// Note that this means it is only responsible for writing one packet.
class ControlPacketWriteRequest {
 public:
  explicit ControlPacketWriteRequest(Packet packet)
      : control_packet_(std::move(packet)) {}

  absl::StatusOr<Packet> NextPacket(int) {
    if (is_started_) {
      return absl::OutOfRangeError("Packet was already sent");
    }
    is_started_ = true;
    return std::move(control_packet_);
  }

 private:
  Packet control_packet_;
  bool is_started_ = false;
};

}  // namespace weave
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_WEAVE_SINGLE_PACKET_WRITE_REQUEST_H_
