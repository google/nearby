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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_INTERNAL_IMPL_G3_DEVICE_INFO_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_INTERNAL_IMPL_G3_DEVICE_INFO_H_

#include <functional>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "fastpair/internal/api/device_info.h"

namespace nearby {
namespace g3 {

class DeviceInfo : public api::DeviceInfo {
 public:
  api::DeviceInfo::OsType GetOsType() const override {
    return api::DeviceInfo::OsType::kChromeOs;
  }

  bool IsScreenLocked() const override { return false; }

  void RegisterScreenLockedListener(
      absl::string_view listener_name,
      std::function<void(api::DeviceInfo::ScreenStatus)> callback) override {
    screen_locked_listeners_.emplace(listener_name, std::move(callback));
  }

  void UnregisterScreenLockedListener(
      absl::string_view listener_name) override {
    screen_locked_listeners_.erase(listener_name);
  }

  absl::flat_hash_map<std::string,
                      std::function<void(api::DeviceInfo::ScreenStatus)>>
      screen_locked_listeners_;
};

}  // namespace g3
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_INTERNAL_IMPL_G3_DEVICE_INFO_H_
