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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_INTERNAL_API_DEVICE_INFO_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_INTERNAL_API_DEVICE_INFO_H_

#include <functional>
#include <string>

#include "absl/strings/string_view.h"

namespace location {
namespace nearby {
namespace api {

class DeviceInfo {
 public:
  enum class ScreenStatus {
    kUndetermined = 0,
    kLocked = 1,
    kUnlocked = 2,
  };

  enum class OsType {
    kUnknown = 0,
    kAndroid = 1,
    kChromeOs = 2,
    kIos = 3,
    kWindows = 4,
  };

  virtual ~DeviceInfo() = default;

  virtual OsType GetOsType() const = 0;

  // Monitor screen status
  virtual bool IsScreenLocked() const = 0;
  virtual void RegisterScreenLockedListener(
      absl::string_view listener_name,
      std::function<void(ScreenStatus)> callback) = 0;
  virtual void UnregisterScreenLockedListener(
      absl::string_view listener_name) = 0;
};

}  // namespace api
}  // namespace nearby
}  // namespace location

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_INTERNAL_API_DEVICE_INFO_H_
