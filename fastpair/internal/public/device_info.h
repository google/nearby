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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_INTERNAL_PUBLIC_DEVICE_INFO_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_INTERNAL_PUBLIC_DEVICE_INFO_H_

#include <functional>
#include <string>

#include "absl/strings/string_view.h"
#include "fastpair/internal/api/device_info.h"
#include "fastpair/internal/api/fast_pair_platform.h"

namespace nearby {
namespace fastpair {

class DeviceInfo {
 public:
  virtual ~DeviceInfo() = default;

  virtual api::DeviceInfo::OsType GetOsType() const = 0;

  virtual bool IsScreenLocked() const = 0;
  virtual void RegisterScreenLockedListener(
      absl::string_view listener_name,
      std::function<void(api::DeviceInfo::ScreenStatus)> callback) = 0;
  virtual void UnregisterScreenLockedListener(
      absl::string_view listener_name) = 0;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_INTERNAL_PUBLIC_DEVICE_INFO_H_
