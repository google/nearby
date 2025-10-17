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
#if TARGET_OS_IPHONE
#import <UIKit/UIKit.h>
#endif  // TARGET_OS_IPHONE

#import "internal/platform/implementation/apple/Log/GNCLogger.h"

namespace nearby {
namespace apple {

AppLifecycleMonitor::AppLifecycleMonitor(
    absl::AnyInvocable<void(AppLifecycleState)> state_updated_callback)
    : api::AppLifecycleMonitor(std::move(state_updated_callback)) {
#if TARGET_OS_IPHONE
  // Get the default notification center
  NSNotificationCenter *center = [NSNotificationCenter defaultCenter];

  // Register for app lifecycle notifications
  background_observer_ =
      [center addObserverForName:UIApplicationDidEnterBackgroundNotification
                          object:nil
                           queue:nil
                      usingBlock:^(NSNotification *notification) {
                        GNCLoggerDebug(@"AppLifecycleMonitor: Did enter foreground.");
                        this->state_updated_callback_(AppLifecycleState::kBackground);
                      }];

  active_observer_ = [center addObserverForName:UIApplicationWillResignActiveNotification
                                         object:nil
                                          queue:nil
                                     usingBlock:^(NSNotification *notification) {
                                       GNCLoggerDebug(@"AppLifecycleMonitor: Will enter inactive.");
                                       this->state_updated_callback_(AppLifecycleState::kInactive);
                                     }];

  inactive_observer_ = [center addObserverForName:UIApplicationDidBecomeActiveNotification
                                           object:nil
                                            queue:nil
                                       usingBlock:^(NSNotification *notification) {
                                         GNCLoggerDebug(@"AppLifecycleMonitor: Will enter active.");
                                         this->state_updated_callback_(AppLifecycleState::kActive);
                                       }];
#endif  // TARGET_OS_IPHONE
}

AppLifecycleMonitor::~AppLifecycleMonitor() {
#if TARGET_OS_IPHONE
  [[NSNotificationCenter defaultCenter] removeObserver:background_observer_];
  [[NSNotificationCenter defaultCenter] removeObserver:active_observer_];
  [[NSNotificationCenter defaultCenter] removeObserver:inactive_observer_];
#endif  // TARGET_OS_IPHONE
}

}  // namespace apple
}  // namespace nearby
