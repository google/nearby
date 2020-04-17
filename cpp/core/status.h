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

#ifndef CORE_STATUS_H_
#define CORE_STATUS_H_

namespace location {
namespace nearby {
namespace connections {

struct Status {
  enum Value {
    SUCCESS,
    ERROR,
    OUT_OF_ORDER_API_CALL,
    ALREADY_HAVE_ACTIVE_STRATEGY,
    ALREADY_ADVERTISING,
    ALREADY_DISCOVERING,
    ENDPOINT_IO_ERROR,
    ENDPOINT_UNKNOWN,
    CONNECTION_REJECTED,
    ALREADY_CONNECTED_TO_ENDPOINT,
    NOT_CONNECTED_TO_ENDPOINT,
    BLUETOOTH_ERROR,
    PAYLOAD_UNKNOWN,
  };
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_STATUS_H_
