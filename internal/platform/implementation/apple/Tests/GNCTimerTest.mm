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

#include "internal/platform/implementation/apple/timer.h"

#import <XCTest/XCTest.h>

#include <memory>

@interface GNCTimerTest : XCTestCase
@end

@implementation GNCTimerTest

- (void)testCreateTimer {
  auto timer = std::make_unique<nearby::apple::Timer>();
  XCTAssertTrue(timer != nullptr);
}

- (void)testStartAndFire {
  XCTestExpectation* expectation = [self expectationWithDescription:@"Timer fired"];
  auto timer = std::make_unique<nearby::apple::Timer>();

  bool fired = false;
  XCTAssertTrue(timer->Create(10, 0, [&]() {
    fired = true;
    [expectation fulfill];
  }));

  [self waitForExpectationsWithTimeout:1.0 handler:nil];
  XCTAssertTrue(fired);
}

- (void)testStop {
  auto timer = std::make_unique<nearby::apple::Timer>();

  bool fired = false;
  XCTAssertTrue(timer->Create(1000, 0, [&]() { fired = true; }));

  XCTAssertTrue(timer->Stop());

  // Wait for a second to ensure the timer doesn't fire.
  [NSRunLoop.currentRunLoop runUntilDate:[NSDate dateWithTimeIntervalSinceNow:1.0]];
  XCTAssertFalse(fired);
}

- (void)testPeriodicTimer {
  XCTestExpectation* expectation = [self expectationWithDescription:@"Timer fired twice"];
  auto timer = std::make_unique<nearby::apple::Timer>();

  int fireCount = 0;
  XCTAssertTrue(timer->Create(10, 10, [&]() {
    fireCount++;
    if (fireCount == 2) {
      [expectation fulfill];
    }
  }));

  [self waitForExpectationsWithTimeout:1.0 handler:nil];
  XCTAssertEqual(fireCount, 2);
  XCTAssertTrue(timer->Stop());
}

- (void)testRestart {
  XCTestExpectation* expectation = [self expectationWithDescription:@"Timer fired"];
  auto timer = std::make_unique<nearby::apple::Timer>();

  bool firstFired = false;
  XCTAssertTrue(timer->Create(1000, 0, [&]() { firstFired = true; }));

  XCTAssertTrue(timer->Stop());

  bool secondFired = false;
  XCTAssertTrue(timer->Create(10, 0, [&]() {
    secondFired = true;
    [expectation fulfill];
  }));

  [self waitForExpectationsWithTimeout:1.0 handler:nil];
  XCTAssertFalse(firstFired);
  XCTAssertTrue(secondFired);
}

- (void)testStopWhenNotRunning {
  auto timer = std::make_unique<nearby::apple::Timer>();
  XCTAssertTrue(timer->Stop());
}

- (void)testFireNow {
  XCTestExpectation* expectation = [self expectationWithDescription:@"Timer fired"];
  auto timer = std::make_unique<nearby::apple::Timer>();

  bool fired = false;
  XCTAssertTrue(timer->Create(1000, 0, [&]() {
    fired = true;
    [expectation fulfill];
  }));

  XCTAssertTrue(timer->FireNow());

  [self waitForExpectationsWithTimeout:1.0 handler:nil];
  XCTAssertTrue(fired);
}

@end
