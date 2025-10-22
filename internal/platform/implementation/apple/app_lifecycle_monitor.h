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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_APPLE_APP_LIFECYCLE_MONITOR_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_APPLE_APP_LIFECYCLE_MONITOR_H_

#import <Foundation/Foundation.h>

#include "internal/platform/implementation/app_lifecycle_monitor.h"

#import "internal/platform/implementation/apple/GNCNotificationCenter.h"

namespace nearby {
namespace apple {

class AppLifecycleMonitor : public api::AppLifecycleMonitor {
 public:
  explicit AppLifecycleMonitor(std::function<void(AppLifecycleState)> state_updated_callback);

  // Test only.
  AppLifecycleMonitor(id<GNCNotificationCenter> notification_center,
                      std::function<void(AppLifecycleState)> state_updated_callback);
  ~AppLifecycleMonitor() override;

 private:
  id<GNCNotificationCenter> notification_center_ = nil;
  id<NSObject> active_observer_ = nil;
  id<NSObject> inactive_observer_ = nil;
  id<NSObject> background_observer_ = nil;
};

}  // namespace apple
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_APPLE_APP_LIFECYCLE_MONITOR_H_
