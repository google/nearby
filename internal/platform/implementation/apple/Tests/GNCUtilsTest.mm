// Copyright 2020 Google LLC
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

#include "internal/platform/byte_array.h"
#include "internal/platform/implementation/apple/utils.h"

using ::nearby::ByteArray;
using ::nearby::ByteArrayFromNSData;
using ::nearby::CppStringFromObjCString;
using ::nearby::ObjCStringFromCppString;

@interface GNCUtilsTest : XCTestCase
@end

@implementation GNCUtilsTest

// Tests that strings can be converted between C++ and Obj-C.
- (void)testStrings {
  XCTAssert([ObjCStringFromCppString(std::string("Hey")) isEqual:@"Hey"]);
  XCTAssert(CppStringFromObjCString(@"Dude") == std::string("Dude"));
}

// Tests that data objects can be converted between C++ and Obj-C.
- (void)testDataObjects {
  uint8_t bytes[] = {0xfe, 0xed, 0xfa, 0xce, 0xde, 0xad, 0xbe, 0xef};
  ByteArray byteArray = ByteArray{(const char *)bytes, sizeof(bytes)};
  NSData *nsData = [NSData dataWithBytes:bytes length:sizeof(bytes)];

  XCTAssert([NSDataFromByteArray(byteArray) isEqual:nsData]);
  XCTAssert(memcmp(ByteArrayFromNSData(nsData).data(), bytes, sizeof(bytes)) == 0);
}

@end
