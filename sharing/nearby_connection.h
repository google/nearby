// Copyright 2022 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_SHARING_NEARBY_CONNECTION_H_
#define THIRD_PARTY_NEARBY_SHARING_NEARBY_CONNECTION_H_

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace nearby {
namespace sharing {

// A socket-like wrapper around Nearby Connections that allows for asynchronous
// reads and writes.
class NearbyConnection {
 public:
  virtual ~NearbyConnection() = default;

  // Reads a stream of bytes from the remote device. Invoke `callback` when
  // there is incoming data or when the socket is closed. Must not be used on
  // an already closed connection.
  // There should only be 1 outstanding Read() call at a time.
  // If packet is already available, `callback` will be invoked immediately.
  virtual void Read(
      std::function<void(std::optional<std::vector<uint8_t>> bytes)>
          callback) = 0;

  // Writes an outgoing stream of bytes to the remote device asynchronously.
  // Must not be used on an already closed connection.
  virtual void Write(std::vector<uint8_t> bytes) = 0;

  // Closes the socket and disconnects from the remote device. This object will
  // be invalidated after `listener` in SetDisconnectionListener is invoked.
  virtual void Close() = 0;

  // Listens to the socket being closed. Invoke `listener` when the socket is
  // closed. This object will be invalidated after `listener` is invoked.
  // Previously set listener will be replaced by `listener`.
  virtual void SetDisconnectionListener(std::function<void()> listener) = 0;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_NEARBY_CONNECTION_H_
