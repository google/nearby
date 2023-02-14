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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_SOCKET_ABSTRACTION_SINGLE_PACKET_WRITE_REQUEST_H_
#define THIRD_PARTY_NEARBY_INTERNAL_SOCKET_ABSTRACTION_SINGLE_PACKET_WRITE_REQUEST_H_

// NOLINTNEXTLINE
#include <future>

#include "absl/status/status.h"
#include "internal/socket_abstraction/packet.h"
#include "internal/socket_abstraction/write_request.h"

namespace nearby {
namespace socket_abstraction {

class SinglePacketWriteRequest : public WriteRequest {
 public:
  explicit SinglePacketWriteRequest(Packet packet) : packet_(packet) {}
  bool IsStarted() override { return started_; }
  bool IsFinished() override { return started_; }
  absl::StatusOr<Packet> NextPacket(int max_packet_size) override {
    if (IsFinished()) {
      return absl::ResourceExhaustedError("Packet was already sent");
    }
    started_ = true;
    return packet_;
  }

  void Reset() override { started_ = false; }
  void SetFuture(absl::Status status) override { result_.set_value(status); }
  std::future<absl::Status> GetFutureResult() override {
    return result_.get_future();
  }

 private:
  std::promise<absl::Status> result_;
  Packet packet_;
  bool started_ = false;
};

}  // namespace socket_abstraction
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_SOCKET_ABSTRACTION_SINGLE_PACKET_WRITE_REQUEST_H_
