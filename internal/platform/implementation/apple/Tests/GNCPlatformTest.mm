// Copyright 2023 Google LLC
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

#include "internal/platform/implementation/platform.h"

#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

void GNCEnsureFileAtPath(std::string path) {
  [NSFileManager.defaultManager
            createDirectoryAtPath:[@(path.c_str()) stringByDeletingLastPathComponent]
      withIntermediateDirectories:YES
                       attributes:nil
                            error:nil];
  [[NSData data] writeToFile:@(path.c_str()) options:0 error:nil];
}

@interface GNCPlatformTest : XCTestCase
@end

@implementation GNCPlatformTest

- (void)testGetCustomSavePath {
  NSString *expected = [NSURL fileURLWithPath:@"a/b/c.d"].path;
  std::string actual = nearby::api::ImplementationPlatform::GetCustomSavePath("a/b", "c.d");
  XCTAssertEqualObjects(@(actual.c_str()), expected);
}

- (void)testGetCustomSavePathWithIllegalCharacters {
  NSString *expected = [NSURL fileURLWithPath:@"a/b/Fi?le*: Name.ext"].path;
  std::string actual =
      nearby::api::ImplementationPlatform::GetCustomSavePath("a/b", "Fi?le*/ Name.ext");
  XCTAssertEqualObjects(@(actual.c_str()), expected);
}

- (void)testGetCustomSavePathWithPathEscapingCharacters {
  NSString *expected = [NSURL fileURLWithPath:@"a/b/..:c:..:d.e"].path;
  std::string actual =
      nearby::api::ImplementationPlatform::GetCustomSavePath("a/../../b", "../c/../d.e");
  XCTAssertEqualObjects(@(actual.c_str()), expected);
}

- (void)testGetCustomSavePathDuplicateNames {
  NSString *expected1 = [NSURL fileURLWithPath:@"a/b/cat.jpg"].path;
  NSString *expected2 = [NSURL fileURLWithPath:@"a/b/cat 2.jpg"].path;
  NSString *expected3 = [NSURL fileURLWithPath:@"a/b/cat 3.jpg"].path;

  std::string actual1 = nearby::api::ImplementationPlatform::GetCustomSavePath("a/b", "cat.jpg");
  GNCEnsureFileAtPath(actual1);

  std::string actual2 = nearby::api::ImplementationPlatform::GetCustomSavePath("a/b", "cat.jpg");
  GNCEnsureFileAtPath(actual2);

  std::string actual3 = nearby::api::ImplementationPlatform::GetCustomSavePath("a/b", "cat.jpg");

  // Cleanup created files.
  [NSFileManager.defaultManager removeItemAtPath:@(actual1.c_str()) error:nil];
  [NSFileManager.defaultManager removeItemAtPath:@(actual2.c_str()) error:nil];

  XCTAssertEqualObjects(@(actual1.c_str()), expected1);
  XCTAssertEqualObjects(@(actual2.c_str()), expected2);
  XCTAssertEqualObjects(@(actual3.c_str()), expected3);
}

- (void)testGetDownloadPath {
  NSString *expected =
      [[NSURL fileURLWithPath:NSTemporaryDirectory()] URLByAppendingPathComponent:@"a/b/c.d"].path;
  std::string actual = nearby::api::ImplementationPlatform::GetDownloadPath("a/b", "c.d");
  XCTAssertEqualObjects(@(actual.c_str()), expected);
}

@end
