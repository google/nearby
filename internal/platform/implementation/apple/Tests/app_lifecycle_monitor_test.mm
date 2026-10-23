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

#if TARGET_OS_IPHONE
#import <UIKit/UIKit.h>
#endif  // TARGET_OS_IPHONE
#import <XCTest/XCTest.h>

#include <memory>

#import "internal/platform/implementation/apple/Tests/GNCFakeNotificationCenter.h"

#include "internal/platform/implementation/app_lifecycle_monitor.h"

using nearby::api::AppLifecycleMonitor;

@interface AppLifecycleMonitorTests : XCTestCase
@end

@implementation AppLifecycleMonitorTests {
  GNCFakeNotificationCenter *_notificationCenter;
  AppLifecycleMonitor::AppLifecycleState _appLifecycleState;
  int _callbackCount;
  std::unique_ptr<nearby::apple::AppLifecycleMonitor> _monitor;
}

- (void)setUp {
  [super setUp];
  _notificationCenter = [[GNCFakeNotificationCenter alloc] init];
  _appLifecycleState = AppLifecycleMonitor::AppLifecycleState::kInactive;
  _monitor = std::make_unique<nearby::apple::AppLifecycleMonitor>(
      _notificationCenter, ^(AppLifecycleMonitor::AppLifecycleState state) {
        _appLifecycleState = state;
        _callbackCount++;
      });
}

- (void)tearDown {
  _monitor.reset();
  [super tearDown];
}

- (void)testAppEntersBackground {
  [_notificationCenter postNotificationName:UIApplicationDidEnterBackgroundNotification object:nil];

  XCTAssertEqual(_appLifecycleState, AppLifecycleMonitor::AppLifecycleState::kBackground);
  XCTAssertEqual(_callbackCount, 1);
}

- (void)testAppBecomesInactive {
  [_notificationCenter postNotificationName:UIApplicationWillResignActiveNotification object:nil];

  XCTAssertEqual(_appLifecycleState, AppLifecycleMonitor::AppLifecycleState::kInactive);
  XCTAssertEqual(_callbackCount, 1);
}

- (void)testAppBecomesActive {
  [_notificationCenter postNotificationName:UIApplicationDidBecomeActiveNotification object:nil];

  XCTAssertEqual(_appLifecycleState, AppLifecycleMonitor::AppLifecycleState::kActive);
  XCTAssertEqual(_callbackCount, 1);
}

- (void)testAppReceivesMultipleNotifications {
  [_notificationCenter postNotificationName:UIApplicationWillResignActiveNotification object:nil];
  XCTAssertEqual(_appLifecycleState, AppLifecycleMonitor::AppLifecycleState::kInactive);
  XCTAssertEqual(_callbackCount, 1);

  [_notificationCenter postNotificationName:UIApplicationDidEnterBackgroundNotification object:nil];
  XCTAssertEqual(_appLifecycleState, AppLifecycleMonitor::AppLifecycleState::kBackground);
  XCTAssertEqual(_callbackCount, 2);

  [_notificationCenter postNotificationName:UIApplicationDidBecomeActiveNotification object:nil];
  XCTAssertEqual(_appLifecycleState, AppLifecycleMonitor::AppLifecycleState::kActive);
  XCTAssertEqual(_callbackCount, 3);
}

- (void)testNoCallbackAfterMonitorDestroyed {
  // Destroy monitor and verify no callbacks are received.
  _monitor.reset();

  [_notificationCenter postNotificationName:UIApplicationDidBecomeActiveNotification object:nil];

  XCTAssertEqual(_appLifecycleState, AppLifecycleMonitor::AppLifecycleState::kInactive);
  XCTAssertEqual(_callbackCount, 0);
}

@end
