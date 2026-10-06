// Copyright 2020 Google LLC
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

#ifndef CORE_INTERNAL_MEDIUMS_WEBRTC_H_
#define CORE_INTERNAL_MEDIUMS_WEBRTC_H_

#include <memory>

#include "connections/implementation/bwu_handler.h"

namespace nearby {
namespace connections {
namespace mediums {

// A non-working base implementation for connecting a data channel between two
// devices via WebRtc.
class WebRtc {
 public:
  virtual ~WebRtc() = default;

  // Returns if WebRtc is available as a medium for nearby to transport data.
  // Runs on @MainThread.
  virtual bool IsAvailable() { return false; }

  virtual bool IsUsingCellular() { return false; }

  virtual std::unique_ptr<BwuHandler> CreateBwuHandler(
      BwuHandler::IncomingConnectionCallback incoming_connection_callback) {
    return nullptr;
  }
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_MEDIUMS_WEBRTC_H_
