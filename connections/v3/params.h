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

#ifndef THIRD_PARTY_NEARBY_CONNECTIONS_V3_PARAMS_H_
#define THIRD_PARTY_NEARBY_CONNECTIONS_V3_PARAMS_H_

#include <string>

#include "connections/v3/listeners.h"
#include "internal/interop/device.h"

namespace nearby {
namespace connections {
namespace v3 {

// Used by Discovery in Core::RequestConnection().
// Used by Advertising in Core::StartAdvertising().
struct ConnectionRequestInfo {
  // local_device  - Identifying information about this device (eg. name,
  //                 device type).
  // listener      - A set of callbacks notified when remote endpoints request a
  //                 connection to this endpoint.
  // This variable is a reference since NearbyDevice is a virtual class type,
  // and we can only retain the information correctly by taking a reference.
  NearbyDevice& local_device;
  v3::ConnectionListener listener;
};

}  // namespace v3
}  // namespace connections
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_CONNECTIONS_V3_PARAMS_H_
