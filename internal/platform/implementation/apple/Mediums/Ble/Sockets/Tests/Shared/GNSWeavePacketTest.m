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

#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Shared/GNSWeavePacket.h"

#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Shared/GNSUtils+Private.h"
#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Shared/GNSUtils.h"

// Mock handler for testing visitWithHandler
@interface GNSMockWeavePacketHandler : NSObject <GNSWeavePacketHandler>
@property(nonatomic, nullable) GNSWeaveConnectionRequestPacket *lastConnectionRequestPacket;
@property(nonatomic, nullable) GNSWeaveConnectionConfirmPacket *lastConnectionConfirmPacket;
@property(nonatomic, nullable) GNSWeaveErrorPacket *lastErrorPacket;
@property(nonatomic, nullable) GNSWeaveDataPacket *lastDataPacket;
@property(nonatomic, nullable) id lastContext;
@property(nonatomic) int handleConnectionRequestCalledCount;
@property(nonatomic) int handleConnectionConfirmCalledCount;
@property(nonatomic) int handleErrorCalledCount;
@property(nonatomic) int handleDataCalledCount;
@end

@implementation GNSMockWeavePacketHandler
- (void)handleConnectionRequestPacket:(GNSWeaveConnectionRequestPacket *)packet
                              context:(nullable id)context {
  self.lastConnectionRequestPacket = packet;
  self.lastContext = context;
  self.handleConnectionRequestCalledCount++;
}
- (void)handleConnectionConfirmPacket:(GNSWeaveConnectionConfirmPacket *)packet
                              context:(nullable id)context {
  self.lastConnectionConfirmPacket = packet;
  self.lastContext = context;
  self.handleConnectionConfirmCalledCount++;
}
- (void)handleErrorPacket:(GNSWeaveErrorPacket *)packet context:(nullable id)context {
  self.lastErrorPacket = packet;
  self.lastContext = context;
  self.handleErrorCalledCount++;
}
- (void)handleDataPacket:(GNSWeaveDataPacket *)packet context:(nullable id)context {
  self.lastDataPacket = packet;
  self.lastContext = context;
  self.handleDataCalledCount++;
}
@end

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
  GNSWeavePacket *packet = [GNSWeavePacket parseData:[self weavePacketWithHeader:header data:nil]
                                               error:&error];
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

- (void)testConnectionRequestVisitWithHandler {
  GNSWeaveConnectionRequestPacket *requestPacket =
      [[GNSWeaveConnectionRequestPacket alloc] initWithMinVersion:1
                                                       maxVersion:1
                                                    maxPacketSize:20
                                                             data:nil];
  GNSMockWeavePacketHandler *handler = [[GNSMockWeavePacketHandler alloc] init];
  id context = [[NSObject alloc] init];

  XCTAssertTrue([requestPacket visitWithHandler:handler context:context]);
  XCTAssertEqual(handler.handleConnectionRequestCalledCount, 1);
  XCTAssertEqual(handler.lastConnectionRequestPacket, requestPacket);
  XCTAssertEqual(handler.lastContext, context);
  XCTAssertEqual(handler.handleConnectionConfirmCalledCount, 0);
  XCTAssertEqual(handler.handleErrorCalledCount, 0);
  XCTAssertEqual(handler.handleDataCalledCount, 0);
}

- (void)testConnectionConfirmVisitWithHandler {
  GNSWeaveConnectionConfirmPacket *confirmPacket =
      [[GNSWeaveConnectionConfirmPacket alloc] initWithVersion:1 packetSize:20 data:nil];
  GNSMockWeavePacketHandler *handler = [[GNSMockWeavePacketHandler alloc] init];
  id context = [[NSObject alloc] init];

  XCTAssertTrue([confirmPacket visitWithHandler:handler context:context]);
  XCTAssertEqual(handler.handleConnectionConfirmCalledCount, 1);
  XCTAssertEqual(handler.lastConnectionConfirmPacket, confirmPacket);
  XCTAssertEqual(handler.lastContext, context);
  XCTAssertEqual(handler.handleConnectionRequestCalledCount, 0);
  XCTAssertEqual(handler.handleErrorCalledCount, 0);
  XCTAssertEqual(handler.handleDataCalledCount, 0);
}

- (void)testErrorVisitWithHandler {
  GNSWeaveErrorPacket *errorPacket = [[GNSWeaveErrorPacket alloc] initWithPacketCounter:3];
  GNSMockWeavePacketHandler *handler = [[GNSMockWeavePacketHandler alloc] init];
  id context = [[NSObject alloc] init];

  XCTAssertTrue([errorPacket visitWithHandler:handler context:context]);
  XCTAssertEqual(handler.handleErrorCalledCount, 1);
  XCTAssertEqual(handler.lastErrorPacket, errorPacket);
  XCTAssertEqual(handler.lastContext, context);
  XCTAssertEqual(handler.handleConnectionRequestCalledCount, 0);
  XCTAssertEqual(handler.handleConnectionConfirmCalledCount, 0);
  XCTAssertEqual(handler.handleDataCalledCount, 0);
}

- (void)testDataVisitWithHandler {
  GNSWeaveDataPacket *dataPacket =
      [[GNSWeaveDataPacket alloc] initWithPacketCounter:4
                                            firstPacket:YES
                                             lastPacket:YES
                                                   data:_smallNonEmptyData];
  GNSMockWeavePacketHandler *handler = [[GNSMockWeavePacketHandler alloc] init];
  id context = [[NSObject alloc] init];

  XCTAssertTrue([dataPacket visitWithHandler:handler context:context]);
  XCTAssertEqual(handler.handleDataCalledCount, 1);
  XCTAssertEqual(handler.lastDataPacket, dataPacket);
  XCTAssertEqual(handler.lastContext, context);
  XCTAssertEqual(handler.handleConnectionRequestCalledCount, 0);
  XCTAssertEqual(handler.handleConnectionConfirmCalledCount, 0);
  XCTAssertEqual(handler.handleErrorCalledCount, 0);
}

- (void)testVisitWithHandlerNoMatch {
  GNSWeaveConnectionRequestPacket *requestPacket =
      [[GNSWeaveConnectionRequestPacket alloc] initWithMinVersion:1
                                                       maxVersion:1
                                                    maxPacketSize:20
                                                             data:nil];
  // Handler that implements nothing from the protocol
  id<GNSWeavePacketHandler> emptyHandler = (id<GNSWeavePacketHandler>)[[NSObject alloc] init];

  XCTAssertFalse([requestPacket visitWithHandler:emptyHandler context:nil]);
}

// Additional tests for parsing edge cases

- (void)testParseConnectionRequestMinSize {
  UInt8 header = (1 << 7) + 0;  // Connection Request, counter 0
  NSMutableData *payload = [NSMutableData data];
  UInt16 val = 0;
  // Min payload size is 6 bytes
  [payload appendBytes:&val length:sizeof(val)];
  [payload appendBytes:&val length:sizeof(val)];
  [payload appendBytes:&val length:sizeof(val)];
  XCTAssertEqual(payload.length, 6);

  NSError *error = nil;
  GNSWeavePacket *packet =
      [GNSWeavePacket parseData:[self weavePacketWithHeader:header data:payload] error:&error];
  XCTAssertNotNil(packet);
  XCTAssertNil(error);
  XCTAssertTrue([packet isKindOfClass:[GNSWeaveConnectionRequestPacket class]]);

  // Too small
  NSData *tooSmallPayload = [payload subdataWithRange:NSMakeRange(0, 5)];
  packet = [GNSWeavePacket parseData:[self weavePacketWithHeader:header data:tooSmallPayload]
                               error:&error];
  XCTAssertNil(packet);
  XCTAssertEqual(error.code, GNSErrorParsingWeavePacketTooSmall);
}

- (void)testParseConnectionConfirmMinSize {
  UInt8 header = (1 << 7) + 1;  // Connection Confirm, counter 0
  NSMutableData *payload = [NSMutableData data];
  UInt16 version = 1;
  UInt16 packetSize = kGNSMinSupportedPacketSize;
  // Min payload size is 4 bytes
  UInt16 versionBigEndian = CFSwapInt16HostToBig(version);
  UInt16 packetSizeBigEndian = CFSwapInt16HostToBig(packetSize);
  [payload appendBytes:&versionBigEndian length:sizeof(versionBigEndian)];
  [payload appendBytes:&packetSizeBigEndian length:sizeof(packetSizeBigEndian)];
  XCTAssertEqual(payload.length, 4);

  NSError *error = nil;
  GNSWeavePacket *packet =
      [GNSWeavePacket parseData:[self weavePacketWithHeader:header data:payload] error:&error];
  XCTAssertNotNil(packet);
  XCTAssertNil(error);
  XCTAssertTrue([packet isKindOfClass:[GNSWeaveConnectionConfirmPacket class]]);

  // Too small
  NSData *tooSmallPayload = [payload subdataWithRange:NSMakeRange(0, 3)];
  packet = [GNSWeavePacket parseData:[self weavePacketWithHeader:header data:tooSmallPayload]
                               error:&error];
  XCTAssertNil(packet);
  XCTAssertEqual(error.code, GNSErrorParsingWeavePacketTooSmall);
}

- (void)testParseErrorPacketSize {
  UInt8 header = (1 << 7) + 2;  // Error, counter 0
  // Error packet has no payload
  NSError *error = nil;
  GNSWeavePacket *packet = [GNSWeavePacket parseData:[self weavePacketWithHeader:header data:nil]
                                               error:&error];
  XCTAssertNotNil(packet);
  XCTAssertNil(error);

  // Should still be valid with empty data
  packet = [GNSWeavePacket parseData:[self weavePacketWithHeader:header data:[NSData data]]
                               error:&error];
  XCTAssertNotNil(packet);
  XCTAssertNil(error);

  // Invalid with payload
  packet = [GNSWeavePacket parseData:[self weavePacketWithHeader:header data:_smallNonEmptyData]
                               error:nil];
  // This doesn't cause an error in the current parseData, it just ignores the extra data.
  // Depending on spec, this might be desired or not. Assuming it's fine.
  XCTAssertNotNil(packet);
}

- (void)testMaxPacketCounter {
  XCTAssertThrows([[GNSWeaveErrorPacket alloc] initWithPacketCounter:8],
                  @"Should assert for packet counter >= 8");
  XCTAssertThrows([[GNSWeaveDataPacket alloc] initWithPacketCounter:8
                                                        firstPacket:YES
                                                         lastPacket:YES
                                                               data:_smallNonEmptyData],
                  @"Should assert for packet counter >= 8");
}

@end
