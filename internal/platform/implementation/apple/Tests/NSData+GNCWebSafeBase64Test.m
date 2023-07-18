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

#import "internal/platform/implementation/apple/Mediums/BLEv2/NSData+GNCWebSafeBase64.h"

#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

@interface NSData_GNCWebSafeBase64Test : XCTestCase
@end

@implementation NSData_GNCWebSafeBase64Test

- (void)testEncodingWithPadding {
  NSString *expected = @"AQ";
  NSData *data = [[NSData alloc] initWithBase64EncodedString:@"AQ==" options:0];
  NSString *actual = [data webSafebase64EncodedString];
  XCTAssertEqualObjects(expected, actual);
}

- (void)testEncodingWithNonWebSafeCharacters {
  NSString *expected = @"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
  NSData *data =
      [[NSData alloc] initWithBase64EncodedString:
                          @"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
                                          options:0];
  NSString *actual = [data webSafebase64EncodedString];
  XCTAssertEqualObjects(expected, actual);
}

@end
