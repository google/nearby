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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_WEAVE_SOCKET_H_
#define THIRD_PARTY_NEARBY_INTERNAL_WEAVE_SOCKET_H_

#include <functional>
#include <string>

#include "absl/status/status.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace weave {

struct SocketCallback {
  std::function<void()> on_connected_cb = []() {
    NEARBY_LOGS(WARNING) << "Unimplemented!";
  };
  std::function<void()> on_disconnected_cb = []() {
    NEARBY_LOGS(WARNING) << "Unimplemented!";
  };
  std::function<void(std::string message)> on_receive_cb = [](std::string) {
    NEARBY_LOGS(WARNING) << "Unimplemented!";
  };
  std::function<void(absl::Status)> on_error_cb = [](absl::Status) {
    NEARBY_LOGS(WARNING) << "Unimplemented!";
  };
};

}  // namespace weave
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_WEAVE_SOCKET_H_
