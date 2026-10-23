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

#import <CommonCrypto/CommonDigest.h>  // For CC_MD5_DIGEST_LENGTH
#import <XCTest/XCTest.h>

#import "internal/platform/implementation/apple/GNCUtils.h"

@interface GNCUtilsTest : XCTestCase
@end

@implementation GNCUtilsTest

- (void)testGNCSha256Data_withRegularData {
  // Use the string "hello" as input data.
  NSData *inputData = [@"hello" dataUsingEncoding:NSUTF8StringEncoding];

  // The expected SHA256 hash for "hello" is
  // 2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824.
  const unsigned char expectedBytes[] = {0x2c, 0xf2, 0x4d, 0xba, 0x5f, 0xb0, 0xa3, 0x0e,
                                         0x26, 0xe8, 0x3b, 0x2a, 0xc5, 0xb9, 0xe2, 0x9e,
                                         0x1b, 0x16, 0x1e, 0x5c, 0x1f, 0xa7, 0x42, 0x5e,
                                         0x73, 0x04, 0x33, 0x62, 0x93, 0x8b, 0x98, 0x24};
  NSData *expectedData = [NSData dataWithBytes:expectedBytes length:CC_SHA256_DIGEST_LENGTH];

  // Call the function under test.
  NSData *actualData = GNCSha256Data(inputData);

  // Verify that the actual result matches the expected hash.
  XCTAssertEqualObjects(actualData, expectedData,
                        @"SHA256 hash for 'hello' did not match the expected value.");
}

- (void)testGNCSha256Data_withEmptyData {
  // Use empty NSData as input.
  NSData *inputData = [NSData data];

  // The expected SHA256 hash for an empty input is
  // e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855.
  const unsigned char expectedBytes[] = {0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14,
                                         0x9a, 0xfb, 0xf4, 0xc8, 0x99, 0x6f, 0xb9, 0x24,
                                         0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b, 0x93, 0x4c,
                                         0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55};
  NSData *expectedData = [NSData dataWithBytes:expectedBytes length:CC_SHA256_DIGEST_LENGTH];

  // Call the function under test.
  NSData *actualData = GNCSha256Data(inputData);

  // Verify that the actual result matches the expected hash.
  XCTAssertEqualObjects(actualData, expectedData,
                        @"SHA256 hash for empty data did not match the expected value.");
}

- (void)testGNCSha256String_withRegularString {
  NSString *inputString = @"hello";
  const unsigned char expectedBytes[] = {0x2c, 0xf2, 0x4d, 0xba, 0x5f, 0xb0, 0xa3, 0x0e,
                                         0x26, 0xe8, 0x3b, 0x2a, 0xc5, 0xb9, 0xe2, 0x9e,
                                         0x1b, 0x16, 0x1e, 0x5c, 0x1f, 0xa7, 0x42, 0x5e,
                                         0x73, 0x04, 0x33, 0x62, 0x93, 0x8b, 0x98, 0x24};
  NSData *expectedData = [NSData dataWithBytes:expectedBytes length:CC_SHA256_DIGEST_LENGTH];

  NSData *actualData = GNCSha256String(inputString);

  XCTAssertEqualObjects(actualData, expectedData,
                        @"SHA256 hash for 'hello' string did not match the expected value.");
}

- (void)testGNCSha256String_withEmptyString {
  NSString *inputString = @"";
  const unsigned char expectedBytes[] = {0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14,
                                         0x9a, 0xfb, 0xf4, 0xc8, 0x99, 0x6f, 0xb9, 0x24,
                                         0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b, 0x93, 0x4c,
                                         0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55};
  NSData *expectedData = [NSData dataWithBytes:expectedBytes length:CC_SHA256_DIGEST_LENGTH];

  NSData *actualData = GNCSha256String(inputString);

  XCTAssertEqualObjects(actualData, expectedData,
                        @"SHA256 hash for an empty string did not match the expected value.");
}
- (void)testGNCMd5Data_withRegularData {
  // Use the string "hello" as input data.
  NSData *inputData = [@"hello" dataUsingEncoding:NSUTF8StringEncoding];

  // The expected MD5 hash for "hello" is 5d41402abc4b2a76b9719d911017c592.
  const unsigned char expectedBytes[] = {0x5d, 0x41, 0x40, 0x2a, 0xbc, 0x4b, 0x2a, 0x76,
                                         0xb9, 0x71, 0x9d, 0x91, 0x10, 0x17, 0xc5, 0x92};
  NSData *expectedData = [NSData dataWithBytes:expectedBytes length:CC_MD5_DIGEST_LENGTH];

  // Call the function under test.
  NSData *actualData = GNCMd5Data(inputData);

  // Verify that the actual result matches the expected hash.
  XCTAssertEqualObjects(actualData, expectedData,
                        @"MD5 hash for 'hello' did not match the expected value.");
}

- (void)testGNCMd5Data_withEmptyData {
  // Use empty NSData as input.
  NSData *inputData = [NSData data];

  // The expected MD5 hash for an empty input is d41d8cd98f00b204e9800998ecf8427e.
  const unsigned char expectedBytes[] = {0xd4, 0x1d, 0x8c, 0xd9, 0x8f, 0x00, 0xb2, 0x04,
                                         0xe9, 0x80, 0x09, 0x98, 0xec, 0xf8, 0x42, 0x7e};
  NSData *expectedData = [NSData dataWithBytes:expectedBytes length:CC_MD5_DIGEST_LENGTH];

  // Call the function under test.
  NSData *actualData = GNCMd5Data(inputData);

  // Verify that the actual result matches the expected hash.
  XCTAssertEqualObjects(actualData, expectedData,
                        @"MD5 hash for empty data did not match the expected value.");
}

- (void)testGNCMd5String_withRegularString {
  // The expected MD5 hash for the string "hello" is 5d41402abc4b2a76b9719d911017c592.
  NSString *inputString = @"hello";
  const unsigned char expectedBytes[] = {0x5d, 0x41, 0x40, 0x2a, 0xbc, 0x4b, 0x2a, 0x76,
                                         0xb9, 0x71, 0x9d, 0x91, 0x10, 0x17, 0xc5, 0x92};
  NSData *expectedData = [NSData dataWithBytes:expectedBytes length:CC_MD5_DIGEST_LENGTH];

  // Call the function under test.
  NSData *actualData = GNCMd5String(inputString);

  // Verify that the actual result matches the expected hash.
  XCTAssertEqualObjects(actualData, expectedData,
                        @"MD5 hash for 'hello' did not match the expected value.");
}

- (void)testGNCMd5String_withEmptyString {
  // The expected MD5 hash for an empty string is d41d8cd98f00b204e9800998ecf8427e.
  NSString *inputString = @"";
  const unsigned char expectedBytes[] = {0xd4, 0x1d, 0x8c, 0xd9, 0x8f, 0x00, 0xb2, 0x04,
                                         0xe9, 0x80, 0x09, 0x98, 0xec, 0xf8, 0x42, 0x7e};
  NSData *expectedData = [NSData dataWithBytes:expectedBytes length:CC_MD5_DIGEST_LENGTH];

  // Call the function under test.
  NSData *actualData = GNCMd5String(inputString);

  // Verify that the actual result matches the expected hash.
  XCTAssertEqualObjects(actualData, expectedData,
                        @"MD5 hash for an empty string did not match the expected value.");
}

- (void)testConvertDataToHexString {
  // Test with nil data.
  XCTAssertEqualObjects(GNCConvertDataToHexString(nil), @"0x",
                        @"Hex string for nil data should be '0x'.");

  // Test with empty data.
  NSData *emptyData = [NSData data];
  XCTAssertEqualObjects(GNCConvertDataToHexString(emptyData), @"0x",
                        @"Hex string for empty data should be '0x'.");

  // Test with sample data.
  const unsigned char bytes[] = {0xDE, 0xAD, 0xBE, 0xEF};
  NSData *sampleData = [NSData dataWithBytes:bytes length:sizeof(bytes)];
  NSString *expectedHexString = @"0xdeadbeef";
  XCTAssertEqualObjects(GNCConvertDataToHexString(sampleData), expectedHexString,
                        @"Hex string for sample data did not match the expected value.");
}

@end
