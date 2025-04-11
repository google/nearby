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

#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCBLEL2CAPPacket.h"

#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

@interface GNCBLEL2CAPPacketTest : XCTestCase
@end

@implementation GNCBLEL2CAPPacketTest

#pragma mark Tests

- (void)testReceivedPacketWithValidCommandOnly {
  uint8_t commandBytes[] = {GNCMBLEL2CAPCommandRequestAdvertisement};
  NSData *receivedData = [NSData dataWithBytes:commandBytes length:1];
  GNCBLEL2CAPPacket *packet = [GNCBLEL2CAPPacket receivedPacket:receivedData];
  XCTAssertNotNil(packet);
  XCTAssertEqual(packet.command, GNCMBLEL2CAPCommandRequestAdvertisement);
  XCTAssertNil(packet.data);
  XCTAssertTrue([packet isValid]);
  XCTAssertTrue([packet isFetchAdvertisementRequest]);
  XCTAssertFalse([packet isDataConnectionRequest]);
}

- (void)testReceivedPacketWithValidCommandAndData {
  uint8_t dataBytes[] = {0x01, 0x02, 0x03, 0x04, 0x05};
  NSData *data = [NSData dataWithBytes:dataBytes length:5];

  uint8_t packetBytes[] = {
      GNCMBLEL2CAPCommandResponseAdvertisement, 0x00, 0x05, 0x01, 0x02, 0x03, 0x04, 0x05};
  NSData *receivedData = [NSData dataWithBytes:packetBytes length:8];
  GNCBLEL2CAPPacket *packet = [GNCBLEL2CAPPacket receivedPacket:receivedData];
  XCTAssertNotNil(packet);
  XCTAssertEqual(packet.command, GNCMBLEL2CAPCommandResponseAdvertisement);
  XCTAssertEqualObjects(packet.data, data);
  XCTAssertTrue([packet isValid]);
  XCTAssertFalse([packet isFetchAdvertisementRequest]);
  XCTAssertFalse([packet isDataConnectionRequest]);
}

- (void)testReceivedPacketWithInvalidDataLength {
  uint8_t packetBytes[] = {GNCMBLEL2CAPCommandResponseAdvertisement, 0x00, 0x05, 0x01, 0x02, 0x03};
  NSData *receivedData = [NSData dataWithBytes:packetBytes length:6];
  GNCBLEL2CAPPacket *packet = [GNCBLEL2CAPPacket receivedPacket:receivedData];
  XCTAssertNil(packet);
}

- (void)testGenerateRequestAdvertisementPacket {
  NSData *serviceIdHash = [NSData dataWithBytes:"testHash" length:8];
  NSData *packetData =
      [GNCBLEL2CAPPacket generateRequestAdvertisementPacketFromServiceIdHash:serviceIdHash];
  XCTAssertNotNil(packetData);

  // Check command
  XCTAssertEqual(((const uint8_t *)packetData.bytes)[0], GNCMBLEL2CAPCommandRequestAdvertisement);

  // Check data length
  XCTAssertEqual(((const uint8_t *)packetData.bytes)[1], 0x00);
  XCTAssertEqual(((const uint8_t *)packetData.bytes)[2], 0x08);

  // Check data
  NSData *extractedData = [packetData subdataWithRange:NSMakeRange(3, 8)];
  XCTAssertEqualObjects(extractedData, serviceIdHash);
}

- (void)testGenerateRequestConnectionDataPacket {
  NSData *packetData = [GNCBLEL2CAPPacket generateRequestConnectionDataPacket];
  XCTAssertNotNil(packetData);

  // Check command
  XCTAssertEqual(((const uint8_t *)packetData.bytes)[0], GNCMBLEL2CAPCommandRequestDataConnection);

  // Check there is no data
  XCTAssertEqual(packetData.length, 1);
}

- (void)testGenerateResponseAdvertisementPacket {
  NSData *advertisementData = [NSData dataWithBytes:"testData" length:8];
  NSData *packetData = [GNCBLEL2CAPPacket
      generateResponseAdvertisementPacketFromAdvertisementData:advertisementData];
  XCTAssertNotNil(packetData);

  // Check command
  XCTAssertEqual(((const uint8_t *)packetData.bytes)[0], GNCMBLEL2CAPCommandResponseAdvertisement);

  // Check data length
  XCTAssertEqual(((const uint8_t *)packetData.bytes)[1], 0x00);
  XCTAssertEqual(((const uint8_t *)packetData.bytes)[2], 0x08);

  // Check data
  NSData *extractedData = [packetData subdataWithRange:NSMakeRange(3, 8)];
  XCTAssertEqualObjects(extractedData, advertisementData);
}

- (void)testGenerateResponseConnectionDataReadyPacket {
  NSData *packetData = [GNCBLEL2CAPPacket generateResponseConnectionDataReadyPacket];
  XCTAssertNotNil(packetData);

  // Check command
  XCTAssertEqual(((const uint8_t *)packetData.bytes)[0],
                 GNCMBLEL2CAPCommandResponseDataConnectionReady);

  // Check there is no data
  XCTAssertEqual(packetData.length, 1);
}

- (void)testIsValid {
  GNCBLEL2CAPPacket *packet1 = [[GNCBLEL2CAPPacket alloc] init];
  packet1.command = GNCMBLEL2CAPCommandRequestAdvertisement;
  XCTAssertTrue([packet1 isValid]);

  GNCBLEL2CAPPacket *packet2 = [[GNCBLEL2CAPPacket alloc] init];
  packet2.command = GNCMBLEL2CAPCommandRequestAdvertisementFinish;
  XCTAssertTrue([packet2 isValid]);

  GNCBLEL2CAPPacket *packet3 = [[GNCBLEL2CAPPacket alloc] init];
  packet3.command = GNCMBLEL2CAPCommandRequestDataConnection;
  XCTAssertTrue([packet3 isValid]);

  GNCBLEL2CAPPacket *packet4 = [[GNCBLEL2CAPPacket alloc] init];
  packet4.command = GNCMBLEL2CAPCommandResponseAdvertisement;
  XCTAssertTrue([packet4 isValid]);

  GNCBLEL2CAPPacket *packet5 = [[GNCBLEL2CAPPacket alloc] init];
  packet5.command = GNCMBLEL2CAPCommandResponseServiceIdNotFound;
  XCTAssertTrue([packet5 isValid]);

  GNCBLEL2CAPPacket *packet6 = [[GNCBLEL2CAPPacket alloc] init];
  packet6.command = GNCMBLEL2CAPCommandResponseDataConnectionReady;
  XCTAssertTrue([packet6 isValid]);

  GNCBLEL2CAPPacket *packet7 = [[GNCBLEL2CAPPacket alloc] init];
  packet7.command = GNCMBLEL2CAPCommandResponseDataConnectionFailure;
  XCTAssertTrue([packet7 isValid]);

  GNCBLEL2CAPPacket *packet8 = [[GNCBLEL2CAPPacket alloc] init];
  packet8.command = 0;
  XCTAssertFalse([packet8 isValid]);
}

- (void)testIsFetchAdvertisementRequest {
  GNCBLEL2CAPPacket *packet = [[GNCBLEL2CAPPacket alloc] init];
  packet.command = GNCMBLEL2CAPCommandRequestAdvertisement;
  XCTAssertTrue([packet isFetchAdvertisementRequest]);

  packet.command = GNCMBLEL2CAPCommandRequestDataConnection;
  XCTAssertFalse([packet isFetchAdvertisementRequest]);
}

- (void)testIsDataConnectionRequest {
  GNCBLEL2CAPPacket *packet = [[GNCBLEL2CAPPacket alloc] init];
  packet.command = GNCMBLEL2CAPCommandRequestDataConnection;
  XCTAssertTrue([packet isDataConnectionRequest]);

  packet.command = GNCMBLEL2CAPCommandRequestAdvertisement;
  XCTAssertFalse([packet isDataConnectionRequest]);
}

@end
