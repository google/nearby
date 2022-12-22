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

#import "internal/platform/implementation/apple/Mediums/ble/Sockets/Source/Shared/GNSWeavePacket.h"

#import "internal/platform/implementation/apple/Mediums/ble/Sockets/Source/Shared/GNSUtils.h"

@interface GNSWeavePacketTest : XCTestCase {
  NSData *_largeNonEmptyData;
  NSData *_smallNonEmptyData;
}
@end

@implementation GNSWeavePacketTest

- (void)setUp {
  _largeNonEmptyData =
      [@"Some non-empty data data is larger than 20 bytes." dataUsingEncoding:NSUTF8StringEncoding];
  _smallNonEmptyData = [@"Small data" dataUsingEncoding:NSUTF8StringEncoding];
}

- (NSData *)weavePacketWithHeader:(UInt8)header data:(NSData *)data {
  NSMutableData *dataPacket = [[NSMutableData alloc] init];
  [dataPacket appendBytes:&header length:sizeof(header)];
  if (data != nil) {
    [dataPacket appendData:[data subdataWithRange:NSMakeRange(0, data.length)]];
  }
  return dataPacket;
}

- (void)testParsePacketCounter {
  for (UInt8 packetCounter = 0; packetCounter < 8; packetCounter++) {
    UInt8 header = (packetCounter
                    << 4);  // 0111 0000, the packetCounter value is contained on 5, 6, and 7 bits.
    NSError *error = nil;
    GNSWeavePacket *parsedPacket =
        [GNSWeavePacket parseData:[self weavePacketWithHeader:header data:_largeNonEmptyData]
                            error:&error];
    XCTAssertNotNil(parsedPacket);
    XCTAssertEqual(packetCounter, parsedPacket.packetCounter);
  }
}

- (void)testParseEmptyPacket {
  NSData *data = [NSData data];
  NSError *error = nil;
  GNSWeavePacket *parsedPacket = [GNSWeavePacket parseData:data error:&error];
  XCTAssertNil(parsedPacket);
  XCTAssertNotNil(error);
  XCTAssertEqual(error.domain, kGNSSocketsErrorDomain);
  XCTAssertEqual(error.code, GNSErrorParsingWeavePacketTooSmall);
}

- (void)testParsingControlPacketTooLarge {
  UInt8 header = (1 << 7);  // 1000 0000
  NSError *error = nil;
  GNSWeavePacket *parsedPacket =
      [GNSWeavePacket parseData:[self weavePacketWithHeader:header data:_largeNonEmptyData]
                          error:&error];
  XCTAssertNil(parsedPacket);
  XCTAssertNotNil(error);
  XCTAssertEqual(error.domain, kGNSSocketsErrorDomain);
  XCTAssertEqual(error.code, GNSErrorParsingWeavePacketTooLarge);
}

- (void)testParseFirstDataPacket {
  UInt8 header = (1 << 3);  // 0000 1000, the packet counter is zero
  NSError *error = nil;
  GNSWeavePacket *parsedPacket =
      [GNSWeavePacket parseData:[self weavePacketWithHeader:header data:_largeNonEmptyData]
                          error:&error];
  XCTAssertNotNil(parsedPacket);
  XCTAssertTrue([parsedPacket isKindOfClass:[GNSWeaveDataPacket class]]);
  GNSWeaveDataPacket *parsedDataPacket = (GNSWeaveDataPacket *)parsedPacket;
  XCTAssertTrue(parsedDataPacket.isFirstPacket);
  XCTAssertFalse(parsedDataPacket.isLastPacket);
  XCTAssertEqualObjects(parsedDataPacket.data, _largeNonEmptyData);
}

- (void)testParseLastDataPacket {
  UInt8 header = (1 << 2);  // 0000 0100, the packet counter is zero
  NSError *error = nil;
  GNSWeavePacket *parsedPacket =
      [GNSWeavePacket parseData:[self weavePacketWithHeader:header data:_largeNonEmptyData]
                          error:&error];
  XCTAssertNotNil(parsedPacket);
  XCTAssertTrue([parsedPacket isKindOfClass:[GNSWeaveDataPacket class]]);
  GNSWeaveDataPacket *parsedDataPacket = (GNSWeaveDataPacket *)parsedPacket;
  XCTAssertFalse(parsedDataPacket.isFirstPacket);
  XCTAssertTrue(parsedDataPacket.isLastPacket);
  XCTAssertEqualObjects(parsedDataPacket.data, _largeNonEmptyData);
}

- (void)testParseOtherDataPacket {
  UInt8 header = 0;  // 0000 0000, the packet counter is zero
  NSError *error = nil;
  GNSWeavePacket *parsedPacket =
      [GNSWeavePacket parseData:[self weavePacketWithHeader:header data:_largeNonEmptyData]
                          error:&error];
  XCTAssertNotNil(parsedPacket);
  XCTAssertTrue([parsedPacket isKindOfClass:[GNSWeaveDataPacket class]]);
  GNSWeaveDataPacket *parsedDataPacket = (GNSWeaveDataPacket *)parsedPacket;
  XCTAssertFalse(parsedDataPacket.isFirstPacket);
  XCTAssertFalse(parsedDataPacket.isLastPacket);
  XCTAssertEqualObjects(parsedDataPacket.data, _largeNonEmptyData);
}

- (void)testBuildAndParseMultipleDataPackets {
  NSMutableData *receivedData = [[NSMutableData alloc] init];
  NSUInteger offset = 0;
  UInt8 packetCounter = 0;
  UInt16 packetSize = 20;
  BOOL isLastPacket = NO;
  while (offset < _largeNonEmptyData.length) {
    GNSWeaveDataPacket *dataPacket =
        [GNSWeaveDataPacket dataPacketWithPacketCounter:packetCounter
                                             packetSize:packetSize
                                                   data:_largeNonEmptyData
                                                 offset:&offset];
    XCTAssertNotNil(dataPacket);
    NSError *error = nil;
    GNSWeavePacket *parsedPacket = [GNSWeavePacket parseData:[dataPacket serialize] error:&error];
    XCTAssertNotNil(parsedPacket);
    XCTAssertTrue([parsedPacket isKindOfClass:[GNSWeaveDataPacket class]]);
    GNSWeaveDataPacket *parsedDataPacket = (GNSWeaveDataPacket *)parsedPacket;
    if (receivedData.length == 0) {
      XCTAssertTrue(parsedDataPacket.isFirstPacket);
    }
    [receivedData appendData:[parsedDataPacket.data
                                 subdataWithRange:NSMakeRange(0, parsedDataPacket.data.length)]];
    packetCounter++;
    isLastPacket = parsedDataPacket.isLastPacket;
  }
  XCTAssertTrue(isLastPacket);
  XCTAssertEqual(offset, _largeNonEmptyData.length);
  XCTAssertEqualObjects(receivedData, _largeNonEmptyData);
  XCTAssertEqualWithAccuracy(packetCounter,
                             ceil(_largeNonEmptyData.length / (float)(packetSize - 1)), 0.1f);
}

- (void)testBuildAndParseSingleDataPacket {
  NSUInteger offset = 0;
  UInt8 packetCounter = 0;
  UInt16 packetSize = 20;
  GNSWeaveDataPacket *dataPacket =
      [GNSWeaveDataPacket dataPacketWithPacketCounter:packetCounter
                                           packetSize:packetSize
                                                 data:_smallNonEmptyData
                                               offset:&offset];

  XCTAssertNotNil(dataPacket);
  NSError *error = nil;
  GNSWeavePacket *parsedPacket = [GNSWeavePacket parseData:[dataPacket serialize] error:&error];
  XCTAssertNotNil(parsedPacket);
  XCTAssertTrue([parsedPacket isKindOfClass:[GNSWeaveDataPacket class]]);
  GNSWeaveDataPacket *parsedDataPacket = (GNSWeaveDataPacket *)parsedPacket;
  XCTAssertTrue(parsedDataPacket.isLastPacket);
  XCTAssertTrue(parsedDataPacket.isFirstPacket);
  XCTAssertEqual(offset, _smallNonEmptyData.length);
  XCTAssertEqualObjects(dataPacket.data, _smallNonEmptyData);
}

- (void)testParseErrorControlPacket {
  // 1000 0010, control packets have the MSB set and, error code is 2. The packet counter is 0.
  UInt8 header = (1 << 7) + 2;
  NSError *error = nil;
  GNSWeavePacket *packet =
      [GNSWeavePacket parseData:[self weavePacketWithHeader:header data:nil] error:&error];
  XCTAssertNotNil(packet);
  XCTAssertTrue([packet isKindOfClass:[GNSWeaveErrorPacket class]]);
  XCTAssertEqual(packet.packetCounter, 0);
}

- (void)testBuildErrorControlPacket {
  UInt8 packetCounter = 7;
  GNSWeaveErrorPacket *errorPacket =
      [[GNSWeaveErrorPacket alloc] initWithPacketCounter:packetCounter];
  NSData *serializedPacket = [errorPacket serialize];
  UInt8 firstByte = *(UInt8 *)serializedPacket.bytes;
  // 1111 0010, control packet bit is set, counter is 7 (111) and error code is 2.
  XCTAssertEqual(firstByte, (1 << 7) + (7 << 4) + 2);

  NSError *error = nil;
  GNSWeavePacket *packet = [GNSWeavePacket parseData:serializedPacket error:&error];
  XCTAssertNotNil(packet);
  XCTAssertTrue([packet isKindOfClass:[GNSWeaveErrorPacket class]]);
  XCTAssertEqual(packet.packetCounter, packetCounter);
}

- (void)testParseConnectionRequestControlPacket {
  // 1000 0000, control packets have the most significat bit (MSB) set and, connection request code
  // is 0. The packet counter is 0.
  UInt8 header = (1 << 7) + 0;
  NSMutableData *payload = [[NSMutableData alloc] init];
  UInt16 minVersion = 0;
  UInt16 maxVersion = 3;
  UInt16 maxPacketSize = 200;

  // The Weave protocol uses big-endian format for the multi-bytes types.
  UInt16 minVersionBigEndian = CFSwapInt16HostToBig(minVersion);
  UInt16 maxVersionBigEndian = CFSwapInt16HostToBig(maxVersion);
  UInt16 maxPacketSizeBigEndian = CFSwapInt16HostToBig(maxPacketSize);
  [payload appendBytes:&minVersionBigEndian length:sizeof(minVersionBigEndian)];
  [payload appendBytes:&maxVersionBigEndian length:sizeof(maxVersionBigEndian)];
  [payload appendBytes:&maxPacketSizeBigEndian length:sizeof(maxPacketSizeBigEndian)];
  NSError *error = nil;
  GNSWeavePacket *packet =
      [GNSWeavePacket parseData:[self weavePacketWithHeader:header data:payload] error:&error];
  XCTAssertNotNil(packet);
  XCTAssertTrue([packet isKindOfClass:[GNSWeaveConnectionRequestPacket class]]);
  XCTAssertEqual(packet.packetCounter, 0);
  GNSWeaveConnectionRequestPacket *connectionRequestPacket =
      (GNSWeaveConnectionRequestPacket *)packet;
  XCTAssertEqual(connectionRequestPacket.minVersion, minVersion);
  XCTAssertEqual(connectionRequestPacket.maxVersion, maxVersion);
  XCTAssertEqual(connectionRequestPacket.maxPacketSize, maxPacketSize);
  XCTAssertNil(connectionRequestPacket.data);
}

- (void)testBuildConnectionRequestControlPacket {
  UInt16 minVersion = 0;
  UInt16 maxVersion = 3;
  UInt16 maxPacketSize = 200;
  NSData *smallPayload = [@"Payload" dataUsingEncoding:NSUTF8StringEncoding];
  GNSWeaveConnectionRequestPacket *requestPacket =
      [[GNSWeaveConnectionRequestPacket alloc] initWithMinVersion:minVersion
                                                       maxVersion:maxVersion
                                                    maxPacketSize:maxPacketSize
                                                             data:smallPayload];
  NSData *serializedPacket = [requestPacket serialize];
  UInt8 firstByte = *(UInt8 *)serializedPacket.bytes;
  XCTAssertEqual(firstByte, (1 << 7));  // 1000 0000

  NSError *error = nil;
  GNSWeavePacket *packet = [GNSWeavePacket parseData:serializedPacket error:&error];
  XCTAssertNotNil(packet);
  XCTAssertTrue([packet isKindOfClass:[GNSWeaveConnectionRequestPacket class]]);
  XCTAssertEqual(packet.packetCounter, 0);
  GNSWeaveConnectionRequestPacket *connectionRequestPacket =
      (GNSWeaveConnectionRequestPacket *)packet;
  XCTAssertEqual(connectionRequestPacket.minVersion, minVersion);
  XCTAssertEqual(connectionRequestPacket.maxVersion, maxVersion);
  XCTAssertEqual(connectionRequestPacket.maxPacketSize, maxPacketSize);
  XCTAssertEqualObjects(connectionRequestPacket.data, smallPayload);
}

- (void)testParseConnectionConfirmControlPacket {
  // 1000 0001, control packets have the MSB set and, connection request code is 1. The packet
  // counter is 0.
  UInt8 header = (1 << 7) + 1;
  NSMutableData *payload = [[NSMutableData alloc] init];
  UInt16 version = 2;
  UInt16 packetSize = 30;

  // The Weave protocol uses big-endian format for the multi-bytes types.
  UInt16 versionBigEndian = CFSwapInt16HostToBig(version);
  UInt16 packetSizeBigEndian = CFSwapInt16HostToBig(packetSize);
  [payload appendBytes:&versionBigEndian length:sizeof(versionBigEndian)];
  [payload appendBytes:&packetSizeBigEndian length:sizeof(packetSizeBigEndian)];
  NSError *error = nil;
  GNSWeavePacket *packet =
      [GNSWeavePacket parseData:[self weavePacketWithHeader:header data:payload] error:&error];
  XCTAssertNotNil(packet);
  XCTAssertTrue([packet isKindOfClass:[GNSWeaveConnectionConfirmPacket class]]);
  XCTAssertEqual(packet.packetCounter, 0);
  GNSWeaveConnectionConfirmPacket *connectionRequestPacket =
      (GNSWeaveConnectionConfirmPacket *)packet;
  XCTAssertEqual(connectionRequestPacket.version, version);
  XCTAssertEqual(connectionRequestPacket.packetSize, packetSize);
  XCTAssertNil(connectionRequestPacket.data);
}

- (void)testBuildConnectionConfirmControlPacket {
  UInt16 version = 30;
  UInt16 packetSize = 21;
  NSData *smallPayload = [@"Payload" dataUsingEncoding:NSUTF8StringEncoding];
  GNSWeaveConnectionConfirmPacket *confirmPacket =
      [[GNSWeaveConnectionConfirmPacket alloc] initWithVersion:version
                                                    packetSize:packetSize
                                                          data:smallPayload];
  NSData *serializedPacket = [confirmPacket serialize];
  UInt8 firstByte = *(UInt8 *)serializedPacket.bytes;
  XCTAssertEqual(firstByte, (1 << 7) + 1);  // 1000 0001

  NSError *error = nil;
  GNSWeavePacket *packet = [GNSWeavePacket parseData:serializedPacket error:&error];
  XCTAssertNotNil(packet);
  XCTAssertTrue([packet isKindOfClass:[GNSWeaveConnectionConfirmPacket class]]);
  XCTAssertEqual(packet.packetCounter, 0);
  GNSWeaveConnectionConfirmPacket *connectionConfirmPacket =
      (GNSWeaveConnectionConfirmPacket *)packet;
  XCTAssertEqual(connectionConfirmPacket.version, version);
  XCTAssertEqual(connectionConfirmPacket.packetSize, packetSize);
  XCTAssertEqualObjects(connectionConfirmPacket.data, smallPayload);
}

@end
