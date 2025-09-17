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

#import "internal/platform/implementation/apple/Mediums/BLE/GNCMBleUtils.h"

#import <XCTest/XCTest.h>

#import "internal/platform/implementation/apple/Mediums/BLE/Tests/GNCFakeSocket.h"

static const NSTimeInterval kTimeout = 1.0;
static const NSTimeInterval kWaitForConnectionTimeout = 6.0;  // Allow for the 5s internal timeout

@interface GNCMBleUtilsTest : XCTestCase
@end

@implementation GNCMBleUtilsTest

- (void)testServiceIDHash {
  NSString *serviceID = @"TestServiceID";
  NSData *hash = GNCMServiceIDHash(serviceID);
  XCTAssertNotNil(hash);
  XCTAssertEqual(hash.length, GNCMBleAdvertisementLengthServiceIDHash);
  // Check for consistency
  NSData *hash2 = GNCMServiceIDHash(serviceID);
  XCTAssertEqualObjects(hash, hash2);
}

- (void)testGenerateAndParseBLEFramesIntroductionPacket {
  NSData *serviceIDHash = [@"123" dataUsingEncoding:NSUTF8StringEncoding];
  NSData *packet = GNCMGenerateBLEFramesIntroductionPacket(serviceIDHash);
  XCTAssertNotNil(packet);

  NSData *parsedHash = GNCMParseBLEFramesIntroductionPacket(packet);
  XCTAssertEqualObjects(parsedHash, serviceIDHash);
}

- (void)testParseBLEFramesIntroductionPacketFailure_NilData {
  NSData *parsedHash = GNCMParseBLEFramesIntroductionPacket(nil);
  XCTAssertNil(parsedHash);
}

- (void)testParseBLEFramesIntroductionPacketFailure_ShortData {
  NSData *invalidPacket = [@"12" dataUsingEncoding:NSUTF8StringEncoding];
  NSData *parsedHash = GNCMParseBLEFramesIntroductionPacket(invalidPacket);
  XCTAssertNil(parsedHash);
}

- (void)testGenerateBLEFramesDisconnectionPacket {
  NSData *serviceIDHash = [@"ABC" dataUsingEncoding:NSUTF8StringEncoding];
  NSData *packet = GNCMGenerateBLEFramesDisconnectionPacket(serviceIDHash);
  XCTAssertNotNil(packet);
  XCTAssertTrue(packet.length > serviceIDHash.length);
}

- (void)testGenerateBLEFramesPacketAcknowledgementPacket {
  NSData *serviceIDHash = [@"DEF" dataUsingEncoding:NSUTF8StringEncoding];
  int receivedSize = 1024;
  NSData *packet = GNCMGenerateBLEFramesPacketAcknowledgementPacket(serviceIDHash, receivedSize);
  XCTAssertNotNil(packet);
  XCTAssertTrue(packet.length > serviceIDHash.length);
}

- (void)testGenerateAndParseBLEL2CAPPacketWithData {
  GNCMBLEL2CAPCommand command = GNCMBLEL2CAPCommandRequestAdvertisement;
  NSData *data = [@"test_data" dataUsingEncoding:NSUTF8StringEncoding];
  NSData *packetData = GNCMGenerateBLEL2CAPPacket(command, data);
  XCTAssertNotNil(packetData);

  GNCMBLEL2CAPPacket *packet = GNCMParseBLEL2CAPPacket(packetData);
  XCTAssertNotNil(packet);
  XCTAssertEqual(packet.command, command);
  XCTAssertEqualObjects(packet.data, data);
}

- (void)testGenerateAndParseBLEL2CAPPacketWithoutData {
  GNCMBLEL2CAPCommand command = GNCMBLEL2CAPCommandRequestAdvertisementFinish;
  NSData *packetData = GNCMGenerateBLEL2CAPPacket(command, nil);
  XCTAssertNotNil(packetData);

  GNCMBLEL2CAPPacket *packet = GNCMParseBLEL2CAPPacket(packetData);
  XCTAssertNotNil(packet);
  XCTAssertEqual(packet.command, command);
  XCTAssertNil(packet.data);
}

- (void)testParseBLEL2CAPPacketInvalidData_Nil {
  GNCMBLEL2CAPPacket *packet = GNCMParseBLEL2CAPPacket(nil);
  XCTAssertNil(packet);
}

- (void)testParseBLEL2CAPPacketInvalidData_Empty {
  GNCMBLEL2CAPPacket *packet = GNCMParseBLEL2CAPPacket([[NSData alloc] init]);
  XCTAssertNil(packet);
}

- (void)testParseBLEL2CAPPacketShortData_NoLength {
  uint8_t commandByte = (uint8_t)GNCMBLEL2CAPCommandRequestAdvertisement;
  NSData *shortPacketData = [NSData dataWithBytes:&commandByte length:1];
  GNCMBLEL2CAPPacket *packet = GNCMParseBLEL2CAPPacket(shortPacketData);
  XCTAssertNotNil(packet);
  XCTAssertEqual(packet.command, GNCMBLEL2CAPCommandRequestAdvertisement);
  XCTAssertNil(packet.data);
}

- (void)testParseBLEL2CAPPacketShortData_IncompleteLength {
  uint8_t bytes[] = {GNCMBLEL2CAPCommandRequestAdvertisement, 0x01};
  NSData *shortPacketData = [NSData dataWithBytes:bytes length:sizeof(bytes)];
  GNCMBLEL2CAPPacket *packet = GNCMParseBLEL2CAPPacket(shortPacketData);
  XCTAssertNil(packet);
}

- (void)testParseBLEL2CAPPacketShortData_IncompleteData {
  uint8_t bytes[] = {GNCMBLEL2CAPCommandRequestAdvertisement, 0x00, 0x05, 0x01, 0x02, 0x03, 0x04};
  NSData *shortPacketData = [NSData dataWithBytes:bytes length:sizeof(bytes)];
  GNCMBLEL2CAPPacket *packet = GNCMParseBLEL2CAPPacket(shortPacketData);
  XCTAssertNil(packet);
}

- (void)testWaitForConnection_Success {
  GNCFakeSocket *fakeSocket = [[GNCFakeSocket alloc] init];

  XCTestExpectation *expectation = [self expectationWithDescription:@"Connection success"];
  GNCMWaitForConnection((GNSSocket *)fakeSocket, ^(BOOL flag) {
    XCTAssertTrue(flag);
    [expectation fulfill];
  });

  // Simulate the connection callback
  [fakeSocket simulateSocketDidConnect];

  [self waitForExpectationsWithTimeout:kTimeout handler:nil];
}

- (void)testWaitForConnection_Failure_Disconnect {
  GNCFakeSocket *fakeSocket = [[GNCFakeSocket alloc] init];

  XCTestExpectation *expectation =
      [self expectationWithDescription:@"Connection failed on disconnect"];
  GNCMWaitForConnection((GNSSocket *)fakeSocket, ^(BOOL flag) {
    XCTAssertFalse(flag);
    [expectation fulfill];
  });

  // Simulate the disconnection callback
  [fakeSocket simulateSocketDidDisconnectWithError:nil];

  [self waitForExpectationsWithTimeout:kTimeout handler:nil];
}

- (void)testWaitForConnection_Failure_Timeout {
  GNCFakeSocket *fakeSocket = [[GNCFakeSocket alloc] init];
  // No delegate call - let it time out

  XCTestExpectation *expectation =
      [self expectationWithDescription:@"Connection failed on timeout"];
  GNCMWaitForConnection((GNSSocket *)fakeSocket, ^(BOOL flag) {
    XCTAssertFalse(flag);
    [expectation fulfill];
  });

  [self waitForExpectationsWithTimeout:kWaitForConnectionTimeout handler:nil];
}

@end
