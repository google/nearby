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

#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

#include <string>

#import "internal/platform/implementation/apple/Mediums/WiFiLAN/GNCIPv4Address.h"

@interface GNCIPv4AddressTest : XCTestCase
@end

@implementation GNCIPv4AddressTest

- (void)testInitFromIntWithZero {
  GNCIPv4Address *address = [GNCIPv4Address addressFromFourByteInt:0];
  XCTAssertEqual(address.byte1, 0);
  XCTAssertEqual(address.byte2, 0);
  XCTAssertEqual(address.byte3, 0);
  XCTAssertEqual(address.byte4, 0);
}

- (void)testInitFromIntWithRealIP {
  GNCIPv4Address *address = [GNCIPv4Address addressFromFourByteInt:2869012672];
  XCTAssertEqual(address.byte1, 192);
  XCTAssertEqual(address.byte2, 168);
  XCTAssertEqual(address.byte3, 1);
  XCTAssertEqual(address.byte4, 171);
}

- (void)testInitFromDataWithZero {
  std::string addressString = "\0\0\0\0";
  NSData *addressData = [NSData dataWithBytes:addressString.data() length:4];
  GNCIPv4Address *address = [GNCIPv4Address addressFromData:addressData];
  XCTAssertEqual(address.byte1, 0);
  XCTAssertEqual(address.byte2, 0);
  XCTAssertEqual(address.byte3, 0);
  XCTAssertEqual(address.byte4, 0);
}

- (void)testInitFromDataWithLeadingEmptyBytes {
  std::string addressString = "\0\0\0\0";
  addressString[3] = static_cast<char>(6);

  NSData *addressData = [NSData dataWithBytes:addressString.data() length:4];
  GNCIPv4Address *address = [GNCIPv4Address addressFromData:addressData];
  XCTAssertEqual(address.byte1, 0);
  XCTAssertEqual(address.byte2, 0);
  XCTAssertEqual(address.byte3, 0);
  XCTAssertEqual(address.byte4, 6);
}

- (void)testInitFromDataWithRealIP {
  std::string addressString = "\0\0\0\0";
  addressString[0] = static_cast<char>(192);
  addressString[1] = static_cast<char>(168);
  addressString[2] = static_cast<char>(1);
  addressString[3] = static_cast<char>(171);

  NSData *addressData = [NSData dataWithBytes:addressString.data() length:4];
  GNCIPv4Address *address = [GNCIPv4Address addressFromData:addressData];
  XCTAssertEqual(address.byte1, 192);
  XCTAssertEqual(address.byte2, 168);
  XCTAssertEqual(address.byte3, 1);
  XCTAssertEqual(address.byte4, 171);
}

- (void)testInitFromDataThrowsForWrongByteSize {
  std::string addressString = "\0";
  NSData *addressData = [NSData dataWithBytes:addressString.data() length:addressString.size()];
  XCTAssertThrows([GNCIPv4Address addressFromData:addressData]);
}

- (void)testDottedRepressentationWithZero {
  GNCIPv4Address *address = [[GNCIPv4Address alloc] initWithByte1:0 byte2:0 byte3:0 byte4:0];
  XCTAssertEqualObjects(address.dottedRepresentation, @"0.0.0.0");
}

- (void)testDottedRepressentationWithRealIP {
  GNCIPv4Address *address = [[GNCIPv4Address alloc] initWithByte1:192 byte2:168 byte3:1 byte4:171];
  XCTAssertEqualObjects(address.dottedRepresentation, @"192.168.1.171");
}

- (void)testBinaryRepressentationWithZero {
  GNCIPv4Address *address = [[GNCIPv4Address alloc] initWithByte1:0 byte2:0 byte3:0 byte4:0];
  const uint8_t *bytes = (uint8_t *)address.binaryRepresentation.bytes;
  XCTAssertEqual(bytes[0], 0);
  XCTAssertEqual(bytes[1], 0);
  XCTAssertEqual(bytes[2], 0);
  XCTAssertEqual(bytes[3], 0);
}

- (void)testBinaryRepressentationWithRealIP {
  GNCIPv4Address *address = [[GNCIPv4Address alloc] initWithByte1:192 byte2:168 byte3:1 byte4:171];
  const uint8_t *bytes = (uint8_t *)address.binaryRepresentation.bytes;
  XCTAssertEqual(bytes[0], 192);
  XCTAssertEqual(bytes[1], 168);
  XCTAssertEqual(bytes[2], 1);
  XCTAssertEqual(bytes[3], 171);
}

@end
