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

#ifndef THIRD_PARTY_NEARBY_CONNECTIONS_V3_LISTENING_RESULT_H_
#define THIRD_PARTY_NEARBY_CONNECTIONS_V3_LISTENING_RESULT_H_

#include <string>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "connections/status.h"
#include "internal/platform/connection_info.h"

namespace nearby {
namespace connections {
namespace v3 {

// Returned from StartListeningForIncomingConnections(). Can be passed to
// requestConnection().
struct ListeningResult {
  std::string endpoint_id;
  std::vector<ConnectionInfoVariant> connection_info;
};

using ListeningResultListener = absl::AnyInvocable<void(
    std::pair<Status, v3::ListeningResult> result) const>;

}  // namespace v3
}  // namespace connections
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_CONNECTIONS_V3_LISTENING_RESULT_H_
