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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_SOCKET_ABSTRACTION_SOCKET_H_
#define THIRD_PARTY_NEARBY_INTERNAL_SOCKET_ABSTRACTION_SOCKET_H_

#include <functional>
// NOLINTNEXTLINE
#include <future>

#include "absl/status/status.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace socket_abstraction {

struct SocketCallback {
  std::function<void()> on_connected_cb = []() {
    NEARBY_LOGS(WARNING) << "Unimplemented!";
  };
  std::function<void()> on_disconnected_cb = []() {
    NEARBY_LOGS(WARNING) << "Unimplemented!";
  };
  std::function<void(ByteArray message)> on_receive_cb = [](ByteArray) {
    NEARBY_LOGS(WARNING) << "Unimplemented!";
  };
  std::function<void(absl::Status)> on_error_cb = [](absl::Status) {
    NEARBY_LOGS(WARNING) << "Unimplemented!";
  };
};

class Socket {
 public:
  virtual ~Socket() = default;
  virtual bool IsConnected() = 0;
  virtual void Connect() = 0;
  virtual void Disconnect() = 0;
  virtual std::future<absl::Status> Write(ByteArray message) = 0;
};

}  // namespace socket_abstraction
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_SOCKET_ABSTRACTION_SOCKET_H_
