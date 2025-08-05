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

@interface UtilsTest : XCTestCase
@end

@implementation UtilsTest

// Tests that strings can be converted between C++ and Obj-C.
- (void)testStrings {
  XCTAssert([ObjCStringFromCppString("Hey") isEqual:@"Hey"]);
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

- (void)testBoolConversion {
  XCTAssertTrue(nearby::CppBoolFromObjCBool(YES));
  XCTAssertFalse(nearby::CppBoolFromObjCBool(NO));
}

- (void)testCharFromNSNumber {
  NSNumber *number = @('A');
  XCTAssertEqual(nearby::CharFromNSNumber(number), 'A');
}

- (void)testUUIDStringFromNSUUID {
  NSString *uuidString = @"E621E1F8-C36C-495A-93FC-0C247A3E6E5F";
  NSUUID *uuid = [[NSUUID alloc] initWithUUIDString:uuidString];
  std::string expectedCppString = [uuidString UTF8String];
  XCTAssertEqual(nearby::UUIDStringFromNSUUID(uuid), expectedCppString);
}

- (void)testBluetoothUUIDConversions {
  std::string uuidString = "0000FEF3-0000-1000-8000-00805F9B34FB";
  CBUUID *cbuuid = [CBUUID UUIDWithString:@(uuidString.c_str())];

  // Test C++ string to CBUUID
  CBUUID *convertedCBUUID = nearby::CBUUIDFromBluetoothUUIDString(uuidString);
  XCTAssertEqualObjects(convertedCBUUID, cbuuid);

  // Test CBUUID to C++ string
  std::string convertedString = nearby::BluetoothUUIDStringFromCBUUID(cbuuid);
  XCTAssertEqual(convertedString, uuidString);
}

- (void)testBluetoothUUIDSetConversions {
  std::set<std::string> cppSet = {"0000180D-0000-1000-8000-00805F9B34FB",
                                  "0000180F-0000-1000-8000-00805F9B34FB"};
  NSSet<CBUUID *> *objcSet =
      [NSSet setWithObjects:[CBUUID UUIDWithString:@"0000180D-0000-1000-8000-00805F9B34FB"],
                            [CBUUID UUIDWithString:@"0000180F-0000-1000-8000-00805F9B34FB"], nil];

  // Test C++ set to NSSet
  NSSet<CBUUID *> *convertedObjcSet = nearby::CBUUIDSetFromBluetoothUUIDStringSet(cppSet);
  XCTAssertEqualObjects(convertedObjcSet, objcSet);

  // Test NSSet to C++ set
  std::set<std::string> convertedCppSet = nearby::BluetoothUUIDStringSetFromCBUUIDSet(objcSet);
  XCTAssertEqual(convertedCppSet.size(), cppSet.size());
  XCTAssert(convertedCppSet == cppSet);
}

- (void)testTxtRecordConversions {
  absl::flat_hash_map<std::string, std::string> cppTxtRecord = {{"key1", "value1"},
                                                                {"key2", "value2"}};
  NSDictionary<NSString *, NSData *> *objcTxtRecord = @{
    @"key1" : [@"value1" dataUsingEncoding:NSUTF8StringEncoding],
    @"key2" : [@"value2" dataUsingEncoding:NSUTF8StringEncoding]
  };

  // Test C++ map to NSDictionary
  NSDictionary<NSString *, NSData *> *convertedObjcDict =
      nearby::NSDictionaryFromCppTxtRecords(cppTxtRecord);
  XCTAssertEqualObjects(convertedObjcDict, objcTxtRecord);

  // Test NSDictionary to C++ map
  absl::flat_hash_map<std::string, std::string> convertedCppMap =
      nearby::AbslHashMapFromObjCTxtRecords(objcTxtRecord);
  XCTAssertEqual(convertedCppMap.size(), cppTxtRecord.size());
  XCTAssert(convertedCppMap == cppTxtRecord);
}

@end
