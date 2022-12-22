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

#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Shared/GNSWeavePacket.h"

#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Shared/GNSUtils+Private.h"

// Constants defined by the Weave BLE protocol.
const UInt8 kGNSMaxPacketCounterValue = 8;
const UInt16 kGNSMinSupportedPacketSize = 20;
const NSUInteger kGNSMaxCentralHandshakeDataSize = 13;
static const NSUInteger kHeaderSize = 1;
static const NSUInteger kMaxControlPacketSize = 20;
static const NSUInteger kMinConnectionRequestPayloadSize = 6;
static const NSUInteger kMinConnectionConfirmPayloadSize = 4;

// Header offsets.
static const UInt8 kPacketTypeOffset = 7;
static const UInt8 kPacketCounterOffset = 4;
static const UInt8 kFirstPacketFlagOffset = 3;
static const UInt8 kLastPacketFlagOffset = 2;

// Bitmasks for the packet headers.
static const UInt8 kPacketTypeBitmask = (1 << 7);                                       // 10000000
static const UInt8 kPacketCounterBitmask = (1 << 6) | (1 << 5) | (1 << 4);              // 01110000
static const UInt8 kControlCommandBitmask = (1 << 3) | (1 << 2) | (1 << 1) | (1 << 0);  // 00001111
static const UInt8 kFirstPacketFlagBitmask = (1 << 3);                                  // 00001000
static const UInt8 kLastPacketFlagBitmask = (1 << 2);                                   // 00000100

static const UInt8 kControlPacketValue = 1;

/**
 * The Weave BLE protocol (go/weave-ble-gatt-transport) has two types of packets: control and data.
 * Both packets contain a 1-byte header followed by a variable size payload. The header
 * bit-structure is the following:
 *
 * 1) Control Packet: TCCCNNNN
 *         T: packet type, 1 for control packets
 *       CCC: packet counter
 *      NNNN: control command number (GNSWeaveControlCommand)
 *
 * 2) Data Packet: TCCCFL00
 *         T: packet type, 0 for data packets
 *       CCC: packet counter
 *         F: bit indicating the first packet of a message
 *         L: bit indication the last packet of a message
 *
 * Note: A single packet message will have both F and L set.
 *
 * The helpers below are used to manipulate the headers.
 **/
static UInt8 ExtractPacketType(UInt8 header) {
  return (header & kPacketTypeBitmask) >> kPacketTypeOffset;
}

static UInt8 ExtractPacketCounter(UInt8 header) {
  return (header & kPacketCounterBitmask) >> kPacketCounterOffset;
}

static UInt8 ExtractControlCommand(UInt8 header) { return (header & kControlCommandBitmask); }

static UInt8 ExtractFirstPacketFlag(UInt8 header) {
  return (header & kFirstPacketFlagBitmask) >> kFirstPacketFlagOffset;
}

static UInt8 ExtractLastPacketFlag(UInt8 header) {
  return (header & kLastPacketFlagBitmask) >> kLastPacketFlagOffset;
}

// The Weave protocol uses big-endian format (network byte order) for multi-byte types.
static UInt16 ExtractUInt16(const void *bytes, size_t offset) {
  return CFSwapInt16BigToHost(*(UInt16 *)(bytes + offset));
}

static NSData *ExtractPayloadData(NSData *payload, size_t offset) {
  if (offset >= payload.length) {
    return nil;
  }
  return [payload subdataWithRange:NSMakeRange(offset, payload.length - offset)];
}

static UInt8 WeaveControlPacketHeader(UInt8 packetCounter, GNSWeaveControlCommand command) {
  return (1 << kPacketTypeOffset) | (packetCounter << kPacketCounterOffset) | command;
}

static UInt8 WeaveDataPacketHeader(UInt8 packetCounter, BOOL firstPacketFlag, BOOL lastPacketFlag) {
  return (packetCounter << kPacketCounterOffset) | (firstPacketFlag << kFirstPacketFlagOffset) |
         (lastPacketFlag << kLastPacketFlagOffset);
}

@interface GNSWeavePacket ()

- (nullable instancetype)initWithPacketCounter:(UInt8)packetCounter NS_DESIGNATED_INITIALIZER;

@end

@implementation GNSWeavePacket

+ (GNSWeavePacket *)parseData:(NSData *)data error:(out __autoreleasing NSError **)outError {
  if (data.length < kHeaderSize) {
    if (outError) {
      *outError = GNSErrorWithCode(GNSErrorParsingWeavePacketTooSmall);
    }
    return nil;
  }
  // The header is the first byte of |data|.
  UInt8 header = *(UInt8 *)data.bytes;
  BOOL isControlPacket = ExtractPacketType(header) == kControlPacketValue;
  UInt8 packetCounter = ExtractPacketCounter(header);

  GNSWeavePacket *parsedPacket = nil;
  if (isControlPacket) {
    if (data.length > kMaxControlPacketSize) {
      if (outError) {
        *outError = GNSErrorWithCode(GNSErrorParsingWeavePacketTooLarge);
      }
      return nil;
    }
    GNSWeaveControlCommand controlCommand = ExtractControlCommand(header);
    switch (controlCommand) {
      case GNSWeaveControlCommandConnectionRequest: {
        NSData *payload = [data subdataWithRange:NSMakeRange(1, data.length - 1)];
        if (payload.length < kMinConnectionRequestPayloadSize) {
          if (outError) {
            *outError = GNSErrorWithCode(GNSErrorParsingWeavePacketTooSmall);
          }
          return nil;
        }
        UInt16 minVersion = ExtractUInt16(payload.bytes, 0);
        UInt16 maxVersion = ExtractUInt16(payload.bytes, sizeof(minVersion));
        UInt16 maxPacketSize =
            ExtractUInt16(payload.bytes, sizeof(minVersion) + sizeof(maxVersion));

        // Extracting the (optional) payload data.
        size_t payloadDataOffset = sizeof(maxVersion) + sizeof(minVersion) + sizeof(maxPacketSize);
        NSData *payloadData = ExtractPayloadData(payload, payloadDataOffset);
        parsedPacket = [[GNSWeaveConnectionRequestPacket alloc] initWithMinVersion:minVersion
                                                                        maxVersion:maxVersion
                                                                     maxPacketSize:maxPacketSize
                                                                              data:payloadData];
        break;
      }
      case GNSWeaveControlCommandConnectionConfirm: {
        NSData *payload = [data subdataWithRange:NSMakeRange(1, data.length - 1)];
        if (payload.length < kMinConnectionConfirmPayloadSize) {
          if (outError) {
            *outError = GNSErrorWithCode(GNSErrorParsingWeavePacketTooSmall);
          }
          return nil;
        }
        UInt16 version = ExtractUInt16(payload.bytes, 0);
        UInt16 packetSize = ExtractUInt16(payload.bytes, sizeof(version));
        size_t payloadDataOffset = sizeof(version) + sizeof(packetSize);

        // Extracting the (optional) payload data.
        NSData *payloadData = ExtractPayloadData(payload, payloadDataOffset);
        parsedPacket = [[GNSWeaveConnectionConfirmPacket alloc] initWithVersion:version
                                                                     packetSize:packetSize
                                                                           data:payloadData];
        break;
      }
      case GNSWeaveControlCommandError: {
        parsedPacket = [[GNSWeaveErrorPacket alloc] initWithPacketCounter:packetCounter];
        break;
      }
    }
  } else {
    parsedPacket = [[GNSWeaveDataPacket alloc]
        initWithPacketCounter:packetCounter
                  firstPacket:ExtractFirstPacketFlag(header)
                   lastPacket:ExtractLastPacketFlag(header)
                         data:[data subdataWithRange:NSMakeRange(1, data.length - 1)]];
  }
  NSAssert(parsedPacket != nil, @"Parsed packet cannot be nil.");
  return parsedPacket;
}

- (instancetype)initWithPacketCounter:(UInt8)packetCounter {
  NSAssert(packetCounter < kGNSMaxPacketCounterValue,
           @"The packetCounter should have at most 3 bits.");
  self = [super init];
  if (self) {
    _packetCounter = packetCounter;
  }
  return self;
}

- (instancetype)init {
  [self doesNotRecognizeSelector:_cmd];
  return nil;
}

- (BOOL)visitWithHandler:(id<GNSWeavePacketHandler>)handler context:(id)context {
  [self doesNotRecognizeSelector:_cmd];
  return NO;
}

- (NSData *)serialize {
  [self doesNotRecognizeSelector:_cmd];
  return nil;
}

- (NSString *)description {
  return [NSString
      stringWithFormat:@"<%@: %p, packet counter: %d>", [self class], self, _packetCounter];
}

@end

@implementation GNSWeaveConnectionRequestPacket

- (instancetype)initWithMinVersion:(UInt16)minVersion
                        maxVersion:(UInt16)maxVersion
                     maxPacketSize:(UInt16)maxPacketSize
                              data:(NSData *)data {
  self = [super initWithPacketCounter:0];
  if (self) {
    _minVersion = minVersion;
    _maxVersion = maxVersion;
    _maxPacketSize = maxPacketSize;
    _data = data;
  }
  return self;
}

- (instancetype)initWithPacketCounter:(UInt8)packetCounter {
  [self doesNotRecognizeSelector:_cmd];
  return nil;
}

- (BOOL)visitWithHandler:(id<GNSWeavePacketHandler>)handler context:(id)context {
  if ([handler respondsToSelector:@selector(handleConnectionRequestPacket:context:)]) {
    [handler handleConnectionRequestPacket:self context:context];
    return YES;
  }
  return NO;
}

- (NSData *)serialize {
  NSMutableData *packet = [NSMutableData data];
  // The Weave protocol uses big-endian format (network byte order) for multi-byte types.
  UInt16 minVersionBigEndian = CFSwapInt16HostToBig(self.minVersion);
  UInt16 maxVersionBigEndian = CFSwapInt16HostToBig(self.maxVersion);
  UInt16 maxPacketSizeBigEndian = CFSwapInt16HostToBig(self.maxPacketSize);
  UInt8 header = WeaveControlPacketHeader(0, GNSWeaveControlCommandConnectionRequest);
  [packet appendBytes:&header length:sizeof(header)];
  [packet appendBytes:&minVersionBigEndian length:sizeof(minVersionBigEndian)];
  [packet appendBytes:&maxVersionBigEndian length:sizeof(maxVersionBigEndian)];
  [packet appendBytes:&maxPacketSizeBigEndian length:sizeof(maxPacketSizeBigEndian)];
  [packet appendData:self.data];
  NSAssert(packet.length <= kMaxControlPacketSize, @"Control packets cannot be larger than %lu",
           (unsigned long)kMaxControlPacketSize);
  return packet;
}

- (NSString *)description {
  return [NSString stringWithFormat:@"<%@: %p, packet counter: %d, min version: %d, max version: "
                                    @"%d, max packet size: %d, data size: %ld>",
                                    [self class], self, self.packetCounter, _minVersion,
                                    _maxVersion, _maxPacketSize, (unsigned long)_data.length];
}

@end

@implementation GNSWeaveConnectionConfirmPacket

- (instancetype)initWithVersion:(UInt16)version packetSize:(UInt16)packetSize data:(NSData *)data {
  NSAssert(packetSize >= kGNSMinSupportedPacketSize, @"The minimum packet size is %ld",
           (long)kGNSMinSupportedPacketSize);
  self = [super initWithPacketCounter:0];
  if (self) {
    _version = version;
    _packetSize = packetSize;
    _data = data;
  }
  return self;
}

- (instancetype)initWithPacketCounter:(UInt8)packetCounter {
  [self doesNotRecognizeSelector:_cmd];
  return nil;
}

- (BOOL)visitWithHandler:(id<GNSWeavePacketHandler>)handler context:(id)context {
  if ([handler respondsToSelector:@selector(handleConnectionConfirmPacket:context:)]) {
    [handler handleConnectionConfirmPacket:self context:context];
    return YES;
  }
  return NO;
}

- (NSData *)serialize {
  NSMutableData *packet = [NSMutableData data];
  // The Weave protocol uses big-endian format (network byte order) encoding for multi-byte types.
  UInt16 versionBigEndian = CFSwapInt16HostToBig(self.version);
  UInt16 packetSizeBigEndian = CFSwapInt16HostToBig(self.packetSize);
  UInt8 header = WeaveControlPacketHeader(0, GNSWeaveControlCommandConnectionConfirm);
  [packet appendBytes:&header length:sizeof(header)];
  [packet appendBytes:&versionBigEndian length:sizeof(versionBigEndian)];
  [packet appendBytes:&packetSizeBigEndian length:sizeof(packetSizeBigEndian)];
  [packet appendData:self.data];
  NSAssert(packet.length <= kMaxControlPacketSize, @"Control packets cannot be larger than %lu",
           (unsigned long)kMaxControlPacketSize);
  return packet;
}

- (NSString *)description {
  return
      [NSString stringWithFormat:
                    @"<%@: %p, packet counter: %d, version: %d, packet size: %d, data size: %ld>",
                    [self class], self, self.packetCounter, _version, _packetSize,
                    (unsigned long)_data.length];
}

@end

@implementation GNSWeaveErrorPacket

- (instancetype)initWithPacketCounter:(UInt8)packetCounter {
  return [super initWithPacketCounter:packetCounter];
}

- (BOOL)visitWithHandler:(id<GNSWeavePacketHandler>)handler context:(id)context {
  if ([handler respondsToSelector:@selector(handleErrorPacket:context:)]) {
    [handler handleErrorPacket:self context:context];
    return YES;
  }
  return NO;
}

- (NSData *)serialize {
  NSMutableData *packet = [NSMutableData data];
  UInt8 header = WeaveControlPacketHeader(self.packetCounter, GNSWeaveControlCommandError);
  [packet appendBytes:&header length:sizeof(header)];
  return packet;
}

@end

@implementation GNSWeaveDataPacket

+ (nullable GNSWeaveDataPacket *)dataPacketWithPacketCounter:(UInt8)packetCounter
                                                  packetSize:(UInt16)packetSize
                                                        data:(NSData *)data
                                                      offset:(NSUInteger *)outOffset {
  NSAssert(packetCounter < kGNSMaxPacketCounterValue, @"The packet has more than 3 bits.");
  NSAssert(data.length == 0 || *outOffset < data.length,
           @"The offset falls outside of the data range, data length: %ld, outOffset: %ld",
           (unsigned long)data.length, (unsigned long)*outOffset);

  BOOL isFirstPacket = (*outOffset == 0);
  NSInteger dataSize = MIN(packetSize - kHeaderSize, data.length - *outOffset);
  BOOL isLastPacket = (data.length <= (*outOffset + dataSize));
  NSData *packetData = [data subdataWithRange:NSMakeRange(*outOffset, dataSize)];
  *outOffset = *outOffset + dataSize;

  return [[GNSWeaveDataPacket alloc] initWithPacketCounter:packetCounter
                                               firstPacket:isFirstPacket
                                                lastPacket:isLastPacket
                                                      data:packetData];
}

- (instancetype)initWithPacketCounter:(UInt8)packetCounter
                          firstPacket:(BOOL)isFirstPacket
                           lastPacket:(BOOL)isLastPacket
                                 data:(nonnull NSData *)data {
  self = [super initWithPacketCounter:packetCounter];
  if (self) {
    _firstPacket = isFirstPacket;
    _lastPacket = isLastPacket;
    _data = data;
  }
  return self;
}

- (instancetype)initWithPacketCounter:(UInt8)packetCounter {
  [self doesNotRecognizeSelector:_cmd];
  return nil;
}

- (BOOL)visitWithHandler:(id<GNSWeavePacketHandler>)handler context:(id)context {
  if ([handler respondsToSelector:@selector(handleDataPacket:context:)]) {
    [handler handleDataPacket:self context:context];
    return YES;
  }
  return NO;
}

- (NSData *)serialize {
  UInt8 header = WeaveDataPacketHeader(self.packetCounter, self.isFirstPacket, self.isLastPacket);
  NSMutableData *packet = [NSMutableData data];
  [packet appendBytes:&header length:sizeof(header)];
  [packet appendData:self.data];
  return packet;
}

- (NSString *)description {
  return [NSString
      stringWithFormat:
          @"<%@: %p, packet counter: %d, first packet: %@, last packet: %@, data length: %ld>",
          [self class], self, self.packetCounter, _firstPacket ? @"YES" : @"NO",
          _lastPacket ? @"YES" : @"NO", (unsigned long)_data.length];
}

@end
