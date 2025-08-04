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

#import <XCTest/XCTest.h>

#import "internal/platform/implementation/apple/Log/GNCLogger.h"

@interface GNCLoggerTest : XCTestCase
@end

@implementation GNCLoggerTest

- (void)testGNCLogDebug {
  // To test debug, run with -v=1 or higher
  GNCLog(__FILE__, __LINE__, GNCLogSeverityDebug, @"Debug message");
  XCTAssertTrue(YES, @"Debug test always passes");
}

- (void)testGNCLogInfo {
  GNCLog(__FILE__, __LINE__, GNCLogSeverityInfo, @"Info message");
  XCTAssertTrue(YES, @"Info test always passes");
}

- (void)testGNCLogWarning {
  GNCLog(__FILE__, __LINE__, GNCLogSeverityWarning, @"Warning message");
  XCTAssertTrue(YES, @"Warning test always passes");
}

- (void)testGNCLogError {
  GNCLog(__FILE__, __LINE__, GNCLogSeverityError, @"Error message");
  XCTAssertTrue(YES, @"Error test always passes");
}

- (void)testGNCLogWithArguments {
  GNCLog(__FILE__, __LINE__, GNCLogSeverityInfo, @"Info message with arg: %@", @"test");
  XCTAssertTrue(YES, @"Info test with arguments always passes");
}

@end
