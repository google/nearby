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

#ifndef CORE_PARAMS_H_
#define CORE_PARAMS_H_

#include <string>

#include "core/listeners.h"
#include "platform/base/byte_array.h"
#include "platform/base/core_config.h"

namespace location {
namespace nearby {
namespace connections {

// Used by Discovery in Core::RequestConnection().
// Used by Advertising in Core::StartAdvertising().
struct DLL_API ConnectionRequestInfo {
  // endpoint_info - Identifing information about this endpoint (eg. name,
  //                 device type).
  // listener      - A set of callbacks notified when remote endpoints request a
  //                 connection to this endpoint.
  ByteArray endpoint_info;
  ConnectionListener listener;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_PARAMS_H_
