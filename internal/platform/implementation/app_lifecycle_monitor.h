// Copyright 2025 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_APP_LIFECYCLE_MONITOR_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_APP_LIFECYCLE_MONITOR_H_

#include <functional>
#include <utility>

namespace nearby {
namespace api {

class AppLifecycleMonitor {
 public:
  // The state of the app lifecycle. The state transition is kBackground ->
  // kInactive -> kActive. Foreground is an umbrella term that describes the app
  // when it is either Inactive or Active.
  enum class AppLifecycleState {
    // The app is active when it starts to receive events.
    kActive,
    // The app is inactive but is still not in background.
    kInactive,
    // The app is background.
    kBackground,
  };

  // Registers callbacks for connection changes. The callbacks will be called
  // when device is connected to a LAN network or when the device is connected
  // to internet.
  explicit AppLifecycleMonitor(
      std::function<void(AppLifecycleState)> state_updated_callback)
      : state_updated_callback_(std::move(state_updated_callback)) {}

  virtual ~AppLifecycleMonitor() = default;

 protected:
  // The callback to be called when the app lifecycle state is updated. In
  // order to make it possible to be called from different methods, use
  // std::function instead of absl::AnyInvocable.
  std::function<void(AppLifecycleState)> state_updated_callback_;
};

}  // namespace api
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_APP_LIFECYCLE_MONITOR_H_
