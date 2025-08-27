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

#ifndef THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_WIFI_ADAPTER_H_
#define THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_WIFI_ADAPTER_H_

#include <array>
#include <functional>
#include <optional>
#include <string>

#include "absl/status/status.h"

namespace nearby {
namespace sharing {
namespace api {

class WifiAdapter {
 public:
  enum class PermissionStatus {
    kUndetermined = 0,
    kSystemDenied,
    kUserDenied,
    kAllowed
  };

  virtual ~WifiAdapter() = default;

  // Indicates whether the adapter is actually present/not disabled by the
  // device firmware or hardware switch on the system.
  virtual bool IsPresent() const = 0;

  // Indicates whether the adapter radio is powered.
  virtual bool IsPowered() const = 0;

  // Requests a change to the adapter radio power. Setting `powered` to true
  // will turn on the radio and false will turn it off. On success,
  // `success_callback` will be called. On failure, `error_callback` will be
  // called.
  virtual void SetPowered(bool powered, std::function<void()> success_callback,
                          std::function<void()> error_callback) = 0;
};

}  // namespace api
}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_API_WIFI_ADAPTER_H_
