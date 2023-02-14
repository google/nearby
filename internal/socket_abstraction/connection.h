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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_SOCKET_ABSTRACTION_CONNECTION_H_
#define THIRD_PARTY_NEARBY_INTERNAL_SOCKET_ABSTRACTION_CONNECTION_H_

#include <functional>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "internal/platform/byte_array.h"

namespace nearby {
namespace socket_abstraction {

struct ConnectionCallback {
  std::function<void(absl::Status)> on_transmit_cb;
  std::function<void(ByteArray)> on_remote_transmit_cb;
  std::function<void()> on_disconnected_cb;
};

class Connection {
 public:
  virtual ~Connection() = default;
  virtual void Initialize(ConnectionCallback callback) = 0;
  virtual int GetMaxPacketSize() = 0;
  virtual void Transmit(ByteArray packet) = 0;
  virtual void Close() = 0;
};

}  // namespace socket_abstraction
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_SOCKET_ABSTRACTION_CONNECTION_H_
