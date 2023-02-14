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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_SOCKET_ABSTRACTION_PACKET_COUNTER_GENERATOR_H_
#define THIRD_PARTY_NEARBY_INTERNAL_SOCKET_ABSTRACTION_PACKET_COUNTER_GENERATOR_H_

#include "internal/socket_abstraction/packet.h"

namespace nearby {
namespace socket_abstraction {

class PacketCounterGenerator {
 public:
  int Next() {
    int nextPacketCounter = packet_counter_;
    Update(packet_counter_);
    return nextPacketCounter;
  }

  void Update(int lastPacketCounter) {
    if (lastPacketCounter == Packet::kMaxPacketCounter) {
      packet_counter_ = 0;
    } else {
      packet_counter_++;
    }
  }

  void Reset() { packet_counter_ = 0; }

 private:
  int packet_counter_ = 0;
};

}  // namespace socket_abstraction
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_SOCKET_ABSTRACTION_PACKET_COUNTER_GENERATOR_H_
