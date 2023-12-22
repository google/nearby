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

#ifndef THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_NETWORK_MONITOR_H_
#define THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_NETWORK_MONITOR_H_

#include <functional>
#include <string>
#include <utility>

namespace nearby {
namespace api {

class NetworkMonitor {
 public:
  enum class ConnectionType : int {
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

  explicit NetworkMonitor(std::function<void(ConnectionType, bool)> callback) {
    callback_ = std::move(callback);
  }

  virtual ~NetworkMonitor() = default;

  // Returns true if connected to an AP (Access Point), not necessarily
  // connected to the internet
  virtual bool IsLanConnected() = 0;

  // Returns the type of connection used currently to access the internet
  virtual ConnectionType GetCurrentConnection() = 0;

 protected:
  std::function<void(ConnectionType, bool)> callback_;
};

}  // namespace api
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_NETWORK_MONITOR_H_
