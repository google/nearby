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

#include <utility>

#include "absl/functional/any_invocable.h"

namespace nearby {
namespace api {

class NetworkMonitor {
 public:
  // Registers callbacks for connection changes. The callbacks will be called
  // when device is connected to a LAN network or when the device is connected
  // to internet.
  NetworkMonitor(
      absl::AnyInvocable<void(bool)> lan_connected_callback,
      absl::AnyInvocable<void(bool)> internet_connected_callback)
      : lan_connected_callback_(std::move(lan_connected_callback)),
        internet_connected_callback_(std::move(internet_connected_callback)){}

  virtual ~NetworkMonitor() = default;

  // Returns true if connected to an AP (Access Point), not necessarily
  // connected to the internet.
  virtual bool IsLanConnected() = 0;

  // Returns true if connected to internet.
  virtual bool IsInternetConnected() = 0;

 protected:
  absl::AnyInvocable<void(bool)> lan_connected_callback_;
  absl::AnyInvocable<void(bool)> internet_connected_callback_;
};

}  // namespace api
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_NETWORK_MONITOR_H_
