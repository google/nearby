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

#import "GoogleToolboxForMac/GTMLogger.h"

enum { kDataLengthBytes = 2 };

@implementation GNCBLEL2CAPPacket

+ (instancetype)receivedPacket:(NSData *)receivedData {
  if (receivedData.length < 1) {
    GTMLoggerError(@"[NEARBY] Invalid packet length: %@", @(receivedData.length));
    return nil;
  }

  GNCBLEL2CAPPacket *packet = [[GNCBLEL2CAPPacket alloc] init];
  const uint8_t *bytes = receivedData.bytes;
  NSUInteger receivedDataLength = receivedData.length;

  // Extract command
  packet.command = (GNCMBLEL2CAPCommand)bytes[0];

  // Extract data
  if (receivedDataLength > 3) {
    // Extract data length (2 bytes, big endian)
    int dataLength = (bytes[1] << 8) | bytes[2];

    // Validate data length
    if (dataLength != (int)(receivedDataLength - 3)) {
      GTMLoggerError(@"[NEARBY] Data length mismatch. Expected: %d, Actual: %lu", dataLength,
                     receivedDataLength - 3);
      return nil;
    }
    if (dataLength > 0) {
      packet.data = [NSData dataWithBytes:(bytes + 3) length:dataLength];
    } else {
      packet.data = nil;
    }
  }

  return packet;
}

+ (NSData *)generateRequestAdvertisementPacketFromServiceIdHash:(NSData *)serviceIdHash {
  return [self generateL2CAPPacketPacketFromCommand:GNCMBLEL2CAPCommandRequestAdvertisement
                                               data:serviceIdHash];
}

+ (NSData *)generateRequestConnectionDataPacket {
  return [self generateL2CAPPacketPacketFromCommand:GNCMBLEL2CAPCommandRequestDataConnection
                                               data:nil];
}

+ (NSData *)generateResponseAdvertisementPacketFromAdvertisementData:
    (NSData *_Nullable)advertisementData {
  return [self generateL2CAPPacketPacketFromCommand:GNCMBLEL2CAPCommandResponseAdvertisement
                                               data:advertisementData];
}

+ (NSData *)generateResponseConnectionDataReadyPacket {
  return [self generateL2CAPPacketPacketFromCommand:GNCMBLEL2CAPCommandResponseDataConnectionReady
                                               data:nil];
}

#pragma mark Private

+ (NSData *)generateL2CAPPacketPacketFromCommand:(GNCMBLEL2CAPCommand)command
                                            data:(NSData *_Nullable)data {
  NSMutableData *packet = [NSMutableData dataWithBytes:(uint8_t *)&command length:1];
  if (data != nil) {
    uint8_t bytes[kDataLengthBytes];
    int value = (int)(data.length);

    // Extract the bytes from the int, assuming big-endian order.
    bytes[1] = (value >> 0) & 0xFF;
    bytes[0] = (value >> 8) & 0xFF;

    NSMutableData *dataPacket = [NSMutableData dataWithBytes:bytes length:kDataLengthBytes];
    [dataPacket appendData:data];
    [packet appendData:dataPacket];
  }
  return packet;
}

- (BOOL)isValid {
  if (_command == GNCMBLEL2CAPCommandRequestAdvertisement ||
      _command == GNCMBLEL2CAPCommandRequestAdvertisementFinish ||
      _command == GNCMBLEL2CAPCommandRequestDataConnection ||
      _command == GNCMBLEL2CAPCommandResponseAdvertisement ||
      _command == GNCMBLEL2CAPCommandResponseServiceIdNotFound ||
      _command == GNCMBLEL2CAPCommandResponseDataConnectionReady ||
      _command == GNCMBLEL2CAPCommandResponseDataConnectionFailure) {
    return YES;
  }
  return NO;
}

- (BOOL)isFetchAdvertisementRequest {
  return _command == GNCMBLEL2CAPCommandRequestAdvertisement;
}

- (BOOL)isDataConnectionRequest {
  return _command == GNCMBLEL2CAPCommandRequestDataConnection;
}

@end
