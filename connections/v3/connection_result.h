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

#ifndef THIRD_PARTY_NEARBY_CONNECTIONS_V3_CONNECTION_RESULT_H_
#define THIRD_PARTY_NEARBY_CONNECTIONS_V3_CONNECTION_RESULT_H_

#include <string>

#include "connections/status.h"
#include "internal/interop/authentication_status.h"

namespace nearby {
namespace connections {
namespace v3 {

struct ConnectionResult {
  nearby::connections::Status status;
};

// These fields should never be empty.
struct InitialConnectionInfo {
  // 4-digit authentication code shown to user derived by UKEY2.
  std::string authentication_digits;
  // Raw 32-byte authentication token derived by UKEY2.
  std::string raw_authentication_token;
  // Specifies if the connection is incoming or outgoing.
  bool is_incoming_connection = false;
  // Result of authentication via the DeviceProvider, if available. Only used
  // for `RequestConnectionV3()`.
  AuthenticationStatus authentication_status = AuthenticationStatus::kUnknown;
};

}  // namespace v3
}  // namespace connections
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_CONNECTIONS_V3_CONNECTION_RESULT_H_
