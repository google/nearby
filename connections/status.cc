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

#include "connections/status.h"

#include <string>

namespace nearby {
namespace connections {

std::string Status::ToString() const {
  switch (value) {
    case Status::kSuccess:
      return "kSuccess";
    case Status::kError:
      return "kError";
    case Status::kOutOfOrderApiCall:
      return "kOutOfOrderApiCall";
    case Status::kAlreadyHaveActiveStrategy:
      return "kAlreadyHaveActiveStrategy";
    case Status::kAlreadyAdvertising:
      return "kAlreadyAdvertising";
    case Status::kAlreadyDiscovering:
      return "kAlreadyDiscovering";
    case Status::kEndpointIoError:
      return "kEndpointIoError";
    case Status::kEndpointUnknown:
      return "kEndpointUnknown";
    case Status::kConnectionRejected:
      return "kConnectionRejected";
    case Status::kAlreadyConnectedToEndpoint:
      return "kAlreadyConnectedToEndpoint";
    case Status::kNotConnectedToEndpoint:
      return "kNotConnectedToEndpoint";
    case Status::kBluetoothError:
      return "kBluetoothError";
    case Status::kBleError:
      return "kBleError";
    case Status::kWifiLanError:
      return "kWifiLanError";
    case Status::kPayloadUnknown:
      return "kPayloadUnknown";
  }
}

}  // namespace connections
}  // namespace nearby
