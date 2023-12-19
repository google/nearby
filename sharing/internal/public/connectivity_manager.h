// Copyright 2021 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_SHARING_INTERNAL_PUBLIC_CONNECTIVITY_MANAGER_H_
#define THIRD_PARTY_NEARBY_SHARING_INTERNAL_PUBLIC_CONNECTIVITY_MANAGER_H_

#include <functional>

#include "absl/strings/string_view.h"

namespace nearby {

class ConnectivityManager {
 public:
  enum class ConnectionType {
    kUnknown = 0,  // A connection exists, but its type is unknown.
                   // Also used as a default value.
    kEthernet = 1,
    kWifi = 2,
    k2G = 3,
    k3G = 4,
    k4G = 5,
    kNone = 6,  // No connection.
    kBluetooth = 7,
    k5G = 8,
    kLast = k5G
  };

  virtual ~ConnectivityManager() = default;

  virtual bool IsLanConnected() = 0;

  virtual ConnectionType GetConnectionType() = 0;

  virtual void RegisterConnectionListener(
      absl::string_view listener_name,
      std::function<void(ConnectionType, bool)>) = 0;
  virtual void UnregisterConnectionListener(
      absl::string_view listener_name) = 0;
};

}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_PUBLIC_CONNECTIVITY_MANAGER_H_
