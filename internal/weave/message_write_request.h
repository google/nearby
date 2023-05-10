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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_WEAVE_MESSAGE_WRITE_REQUEST_H_
#define THIRD_PARTY_NEARBY_INTERNAL_WEAVE_MESSAGE_WRITE_REQUEST_H_

#include <string>

#include "internal/platform/future.h"
#include "internal/weave/packet.h"

namespace nearby {
namespace weave {

// Implementation of WriteRequest interface that exists to serialize messages
// into Weave packets and track the sending progress.
//
// This class is not thread-safe, calls to this class will be serialized at the
// socket level.
class MessageWriteRequest {
 public:
  explicit MessageWriteRequest(absl::string_view message);
  MessageWriteRequest(MessageWriteRequest&& other) = default;
  MessageWriteRequest& operator=(MessageWriteRequest&& other) = default;

  bool IsStarted() const;
  bool IsFinished() const;

  absl::StatusOr<Packet> NextPacket(int max_packet_size);

  // Gets the future result, the socket will set this result in the future.
  // The socket only sets the result once the request has been completed.
  nearby::Future<absl::Status> GetWriteStatusFuture() {
    return write_request_status_;
  }

  // This function sets the nearby::Future instance from GetResultFuture() and
  // is called when the caller manages to successfully write the packets in
  // this WriteRequest (as indicated by IsFinished()), or when the caller learns
  // that a packet could not be sent across the wire due to an error or
  // disconnection.
  void SetWriteStatus(absl::Status status) {
    write_request_status_.Set(status);
  }

  // Two message write requests are the same if they have the same message,
  // even if they have different progress members due to referencing and
  // other quirks.
  bool operator==(const MessageWriteRequest& other) const {
    return message_ == other.message_;
  }

  bool operator!=(const MessageWriteRequest& other) const {
    return !(*this == other);
  }

 private:
  std::string message_;
  int position_;
  nearby::Future<absl::Status> write_request_status_;
};

}  // namespace weave
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_WEAVE_MESSAGE_WRITE_REQUEST_H_
