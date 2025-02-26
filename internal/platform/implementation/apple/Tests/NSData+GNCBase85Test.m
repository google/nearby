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

#import "internal/platform/implementation/apple/Mediums/BLEv2/NSData+GNCBase85.h"

#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

@interface NSData_GNCBase85Test : XCTestCase
@end

@implementation NSData_GNCBase85Test

- (void)testEncoding {
  NSString *expected = @"@:E^";
  NSData *data = [@"abc" dataUsingEncoding:NSUTF8StringEncoding];
  NSString *actual = [data base85EncodedString];
  XCTAssertEqualObjects(expected, actual);
}

- (void)testEncodingDataWithZero {
  NSString *expected = @"z";
  NSData *data = [@"\0\0" dataUsingEncoding:NSUTF8StringEncoding];
  NSString *actual = [data base85EncodedString];
  XCTAssertEqualObjects(expected, actual);
}

- (void)testDecoding {
  NSData *data = [@"abc" dataUsingEncoding:NSUTF8StringEncoding];
  NSString *actual = [data base85EncodedString];
  NSData *decodedData = [[NSData alloc] initWithBase85EncodedString:actual];
  XCTAssertEqualObjects(decodedData, data);
}

- (void)testEncodingEmptyData {
  XCTAssertEqualObjects([[@"" dataUsingEncoding:NSUTF8StringEncoding] base85EncodedString], @"");
}

- (void)testDecodingEmptyData {
  XCTAssertEqualObjects([[NSData alloc] initWithBase85EncodedString:@""],
                        [@"" dataUsingEncoding:NSUTF8StringEncoding]);
}

- (void)testDecodingInvalidString {
  NSString *invalidString = @"invalid";
  XCTAssertNil([[NSData alloc] initWithBase85EncodedString:invalidString]);
}

@end
