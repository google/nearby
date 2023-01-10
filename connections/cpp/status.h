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

#include <string>

namespace nearby {
namespace connections {

// Protocol operation result: kSuccess, if operation was successful;
// descriptive error code otherwise.
struct Status {
  // Status is a struct, so it is possible to pass some context about failure,
  // by adding extra fields to it when necessary, and not change any of the
  // method signatures.
  enum Value {
    kSuccess,
    kError,
    kOutOfOrderApiCall,
    kAlreadyHaveActiveStrategy,
    kAlreadyAdvertising,
    kAlreadyDiscovering,
    kEndpointIoError,
    kEndpointUnknown,
    kConnectionRejected,
    kAlreadyConnectedToEndpoint,
    kNotConnectedToEndpoint,
    kBluetoothError,
    kBleError,
    kWifiLanError,
    kPayloadUnknown,
  };
  Value value{kError};
  bool Ok() const { return value == kSuccess; }

  // Converts the status to a logging-friendly string.
  std::string ToString() const;
};

inline bool operator==(const Status& a, const Status& b) {
  return a.value == b.value;
}

inline bool operator!=(const Status& a, const Status& b) { return !(a == b); }

}  // namespace connections
}  // namespace nearby

#endif  // CORE_STATUS_H_
