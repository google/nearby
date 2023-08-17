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
  NSString *actual = [data webSafeBase64EncodedString];
  XCTAssertEqualObjects(expected, actual);
}

- (void)testEncodingWithNonWebSafeCharacters {
  NSString *expected = @"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
  NSData *data =
      [[NSData alloc] initWithBase64EncodedString:
                          @"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
                                          options:0];
  NSString *actual = [data webSafeBase64EncodedString];
  XCTAssertEqualObjects(expected, actual);
}

- (void)testDecodingWithoutPadding {
  NSData *expected = [[NSData alloc] initWithBase64EncodedString:@"AQ==" options:0];
  NSData *actual = [[NSData alloc] initWithWebSafeBase64EncodedString:@"AQ"];
  XCTAssertNotNil(actual);
  XCTAssertEqualObjects(expected, actual);
}

- (void)testDecodingNoPad {
  NSData *expected = [[NSData alloc] initWithBase64EncodedString:@"aaaa" options:0];
  NSData *actual = [[NSData alloc] initWithWebSafeBase64EncodedString:@"aaaa"];
  XCTAssertNotNil(actual);
  XCTAssertEqualObjects(expected, actual);
}

- (void)testDecoding1Pad {
  NSData *expected = [[NSData alloc] initWithBase64EncodedString:@"aaa=" options:0];
  NSData *actual = [[NSData alloc] initWithWebSafeBase64EncodedString:@"aaa"];
  XCTAssertNotNil(actual);
  XCTAssertEqualObjects(expected, actual);
}

- (void)testDecoding2Pad {
  NSData *expected = [[NSData alloc] initWithBase64EncodedString:@"aa==" options:0];
  NSData *actual = [[NSData alloc] initWithWebSafeBase64EncodedString:@"aa"];
  XCTAssertNotNil(actual);
  XCTAssertEqualObjects(expected, actual);
}

- (void)testDecodingSingleCharacterInLastQuadruple {
  NSData *actual = [[NSData alloc] initWithWebSafeBase64EncodedString:@"aaaab"];
  XCTAssertNil(actual);
}

- (void)testDecodingWithAllValidCharacters {
  NSData *expected =
      [[NSData alloc] initWithBase64EncodedString:
                          @"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
                                          options:0];
  NSData *actual =
      [[NSData alloc] initWithWebSafeBase64EncodedString:
                          @"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"];
  XCTAssertNotNil(actual);
  XCTAssertEqualObjects(expected, actual);
}

- (void)testDecodingWithIllegalCharacters {
  NSData *actual = [[NSData alloc] initWithWebSafeBase64EncodedString:@"@#$^&*()"];
  XCTAssertNil(actual);
}

@end
