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

#import "internal/platform/implementation/apple/app_lifecycle_monitor.h"

#import <Foundation/Foundation.h>

#include <utility>

#if TARGET_OS_IPHONE
#import <UIKit/UIKit.h>
#endif  // TARGET_OS_IPHONE

#import "internal/platform/implementation/apple/GNCNotificationCenter.h"
#import "internal/platform/implementation/apple/Log/GNCLogger.h"

namespace nearby {
namespace apple {

AppLifecycleMonitor::AppLifecycleMonitor(
    std::function<void(AppLifecycleState)> state_updated_callback)
    : AppLifecycleMonitor([NSNotificationCenter defaultCenter], std::move(state_updated_callback)) {
}

AppLifecycleMonitor::AppLifecycleMonitor(
    id<GNCNotificationCenter> notification_center,
    std::function<void(AppLifecycleState)> state_updated_callback)
    : api::AppLifecycleMonitor(std::move(state_updated_callback)),
      notification_center_(notification_center) {
  // Copy the callback to be captured by the blocks below. This avoids capturing `this` in the
  // blocks, which would lead to a dangling pointer if `this` is destroyed. The block will hold a
  // copy of the callback.
  std::function<void(AppLifecycleState)> callback = state_updated_callback_;
#if TARGET_OS_IPHONE
  // Register for app lifecycle notifications
  background_observer_ =
      [notification_center_ addObserverForName:UIApplicationDidEnterBackgroundNotification
                                        object:nil
                                         queue:nil
                                    usingBlock:^(NSNotification *notification) {
                                      GNCLoggerDebug(@"AppLifecycleMonitor: Did enter background.");
                                      if (callback) {
                                        callback(AppLifecycleState::kBackground);
                                      }
                                    }];

  inactive_observer_ =
      [notification_center_ addObserverForName:UIApplicationWillResignActiveNotification
                                        object:nil
                                         queue:nil
                                    usingBlock:^(NSNotification *notification) {
                                      GNCLoggerDebug(@"AppLifecycleMonitor: Will enter inactive.");
                                      if (callback) {
                                        callback(AppLifecycleState::kInactive);
                                      }
                                    }];

  active_observer_ =
      [notification_center_ addObserverForName:UIApplicationDidBecomeActiveNotification
                                        object:nil
                                         queue:nil
                                    usingBlock:^(NSNotification *notification) {
                                      GNCLoggerDebug(@"AppLifecycleMonitor: Will enter active.");
                                      if (callback) {
                                        callback(AppLifecycleState::kActive);
                                      }
                                    }];
#else
  GNCLoggerError(@"Not implemented");
#endif  // TARGET_OS_IPHONE
}

AppLifecycleMonitor::~AppLifecycleMonitor() {
#if TARGET_OS_IPHONE
  if (notification_center_) {
    [notification_center_ removeObserver:background_observer_];
    [notification_center_ removeObserver:active_observer_];
    [notification_center_ removeObserver:inactive_observer_];
  }
#endif  // TARGET_OS_IPHONE
}

}  // namespace apple
}  // namespace nearby
