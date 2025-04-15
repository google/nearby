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

#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCBLEL2CAPConnection.h"

#import <Foundation/Foundation.h>

#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCBLEL2CAPStream.h"
#import "internal/platform/implementation/apple/Mediums/Ble/GNCMBleUtils.h"
#import "internal/platform/implementation/apple/Mediums/GNCLeaks.h"
#import "internal/platform/implementation/apple/Mediums/GNCMConnection.h"
#import "GoogleToolboxForMac/GTMLogger.h"

enum { kL2CapPacketLength = 4 };

static NSData *PrefixDataWithServiceIDHash(NSData *serviceIDHash, NSData *data) {
  NSMutableData *combinedData = [NSMutableData dataWithCapacity:serviceIDHash.length + data.length];

  [combinedData appendData:serviceIDHash];
  [combinedData appendData:data];

  return combinedData;
}

static NSData *PrefixLengthData(NSData *data) {
  uint32_t dataLength = (uint32_t)data.length;
  uint32_t lengthBigEndian = CFSwapInt32HostToBig(dataLength);

  NSMutableData *packet = [NSMutableData dataWithCapacity:sizeof(uint32_t)];
  [packet appendBytes:&lengthBigEndian length:sizeof(uint32_t)];
  [packet appendData:data];

  return packet;
}

@interface GNCBLEL2CAPConnection () <GNCBLEL2CAPStreamDelegate>
@property(nonatomic) dispatch_queue_t selfQueue;
@property(nonatomic) GNCBLEL2CAPStream *stream;
@property(nonatomic) NSData *serviceIDHash;
@property(nonatomic) dispatch_queue_t callbackQueue;
@property(nonatomic) BOOL incomingConnection;
@property(nonatomic) BOOL handledIncomingConnectionL2CapPacket;
@property(nonatomic) BOOL handledOutgoingConnectionL2CapPacket;
@property(nonatomic) BOOL handledReceivedL2CapRequestDataConnectionPacket;
@property(nonatomic) BOOL handledReceivedL2CapResponseDataConnectionReadyPacket;
@property(nonatomic) BOOL handledReceivedBLEIntroPacket;
@end

@implementation GNCBLEL2CAPConnection

+ (instancetype)connectionWithStream:(GNCBLEL2CAPStream *)stream
                           serviceID:(nullable NSString *)serviceID
                  incomingConnection:(BOOL)incomingConnection
                       callbackQueue:(dispatch_queue_t)callbackQueue {
  GNCBLEL2CAPConnection *connection = [[GNCBLEL2CAPConnection alloc] init];
  connection.stream = stream;
  stream.delegate = connection;
  connection.serviceIDHash = serviceID ? GNCMServiceIDHash(serviceID) : nil;
  connection.callbackQueue = callbackQueue;
  connection.selfQueue =
      dispatch_queue_create("GNCMBLEL2capConnectionQueue", DISPATCH_QUEUE_SERIAL);
  connection.incomingConnection = incomingConnection;
  return connection;
}

- (void)dealloc {
  [_stream close];
  GNCVerifyDealloc(_stream, 2);  // assert if not deallocated
}

#pragma mark GNCBLEL2CAPConnection

- (void)sendData:(NSData *)data completion:(void (^)(BOOL))completion {
  dispatch_async(_selfQueue, ^{
    NSData *packet;

    // Prefix the service ID hash.
    packet = PrefixLengthData(PrefixDataWithServiceIDHash(_serviceIDHash, data));
    GTMLoggerInfo(@"[NEARBY] GNCBLEL2CAPConnection data to be sent: %@", [packet description]);
    [_stream sendData:packet
        completionBlock:^(BOOL result) {
          dispatch_async(_callbackQueue, ^{
            completion(result);
          });
        }];
  });
}

#pragma mark GNCBLEL2CAPStreamDelegate

- (void)stream:(GNCBLEL2CAPStream *)stream didReceiveData:(NSData *)data {
  GTMLoggerDebug(@"[NEARBY] BLEL2CAPConnection didReceiveData, data: %@", [data description]);

  NSData *realData = [self extractRealDataFromData:data];
  if (realData == nil) {
    return;
  }

  // Validate the L2Cap packet.
  // TODO: b/399815436 - Refactor the validation logic to connections layer.
  if (_incomingConnection) {
    if (!_handledIncomingConnectionL2CapPacket) {
      [self validateIncomingConnectionL2CapPacketFromData:realData];
      return;
    }
    if (!_handledReceivedBLEIntroPacket) {
      [self validateBLEIntroPacketFromData:realData];
      return;
    }
  } else {
    if (!_handledOutgoingConnectionL2CapPacket) {
      [self validateOutgoingConnectionL2CapPacketFromData:realData];
      return;
    }
  }

  // Extract the service ID prefix from each data packet and validate it.
  NSUInteger prefixLength = _serviceIDHash.length;
  if (![[realData subdataWithRange:NSMakeRange(0, prefixLength)] isEqual:_serviceIDHash]) {
    GTMLoggerInfo(@"[NEARBY]: Received wrong data packet and discarded");
    return;
  }

  dispatch_async(_selfQueue, ^{
    if (_connectionHandlers.payloadHandler) {
      dispatch_async(_callbackQueue, ^{
        _connectionHandlers.payloadHandler([NSData
            dataWithData:[realData subdataWithRange:NSMakeRange(prefixLength,
                                                                realData.length - prefixLength)]]);
      });
    }
  });
}

- (void)stream:(GNCBLEL2CAPStream *)stream didDisconnectWithError:(NSError *_Nullable)error {
  dispatch_async(_selfQueue, ^{
    if (_connectionHandlers.disconnectedHandler) {
      dispatch_async(_callbackQueue, ^{
        _connectionHandlers.disconnectedHandler();
      });
    }
  });
}

#pragma mark Private

- (NSData *_Nullable)extractRealDataFromData:(NSData *)data {
  if (data.length < kL2CapPacketLength) {
    GTMLoggerError(@"[NEARBY] Packet length mismatch. Expected: > %d, Actual: %lu",
                   kL2CapPacketLength, data.length);
    return nil;
  }
  int realDataLength = CFSwapInt32BigToHost(
      *(int *)([[data subdataWithRange:NSMakeRange(0, kL2CapPacketLength)] bytes]));
  if (realDataLength != (data.length - kL2CapPacketLength)) {
    GTMLoggerError(@"[NEARBY] Data length mismatch. Expected: %d, Actual: %lu", realDataLength,
                   data.length - kL2CapPacketLength);
    return nil;
  }
  return [data subdataWithRange:NSMakeRange(kL2CapPacketLength, data.length - kL2CapPacketLength)];
}

- (void)validateIncomingConnectionL2CapPacketFromData:(NSData *)data {
  GNCMBleL2CapPacket *L2CapPacket = GNCMParseBleL2CapPacket(data);
  if (!L2CapPacket) {
    return;
  }
  switch (L2CapPacket->command) {
    case GNCMBleL2CapCommandRequestDataConnection: {
      __block NSData *packet = PrefixLengthData(
          GNCMGenerateBleL2CapPacket(GNCMBleL2CapCommandResponseDataConnectionReady, nil));
      __weak __typeof__(self) weakSelf = self;
      dispatch_async(_selfQueue, ^{
        __strong __typeof__(weakSelf) strongSelf = weakSelf;
        if (!strongSelf) {
          return;
        }
        [strongSelf->_stream sendData:packet
                      completionBlock:^(BOOL result){
                      }];
      });
      _handledReceivedL2CapRequestDataConnectionPacket = YES;
    } break;
    case GNCMBleL2CapCommandRequestAdvertisement:
    case GNCMBleL2CapCommandRequestAdvertisementFinish:
    case GNCMBleL2CapCommandResponseAdvertisement:
    case GNCMBleL2CapCommandResponseServiceIdNotFound:
    case GNCMBleL2CapCommandResponseDataConnectionReady:
    case GNCMBleL2CapCommandResponseDataConnectionFailure:
    default:
      break;  // fall through
  }
  _handledIncomingConnectionL2CapPacket = _handledReceivedL2CapRequestDataConnectionPacket;
}

- (void)validateOutgoingConnectionL2CapPacketFromData:(NSData *)data {
  GNCMBleL2CapPacket *L2CapPacket = GNCMParseBleL2CapPacket(data);
  if (!L2CapPacket) {
    return;
  }
  switch (L2CapPacket->command) {
    case GNCMBleL2CapCommandResponseDataConnectionReady: {
      _handledReceivedL2CapResponseDataConnectionReadyPacket = YES;
      NSData *introData = GNCMGenerateBLEFramesIntroductionPacket(_serviceIDHash);
      __weak __typeof__(self) weakSelf = self;
      dispatch_async(_selfQueue, ^{
        __strong __typeof__(weakSelf) strongSelf = weakSelf;
        if (!strongSelf) {
          return;
        }
        [strongSelf->_stream sendData:introData
                      completionBlock:^(BOOL result){
                      }];
      });
    } break;
    case GNCMBleL2CapCommandRequestAdvertisement:
    case GNCMBleL2CapCommandRequestDataConnection:
    case GNCMBleL2CapCommandRequestAdvertisementFinish:
    case GNCMBleL2CapCommandResponseAdvertisement:
    case GNCMBleL2CapCommandResponseServiceIdNotFound:
    case GNCMBleL2CapCommandResponseDataConnectionFailure:
    default:
      break;  // fall through
  }
  _handledOutgoingConnectionL2CapPacket = _handledReceivedL2CapResponseDataConnectionReadyPacket;
}

- (void)validateBLEIntroPacketFromData:(NSData *)data {
  NSData *serviceIDHash = GNCMParseBLEFramesIntroductionPacket(data);
  if ([serviceIDHash isEqual:_serviceIDHash]) {
    _handledReceivedBLEIntroPacket = YES;
    GTMLoggerInfo(@"[NEARBY]: Received BLE intro packet and handled.");
  } else {
    GTMLoggerInfo(@"[NEARBY]: Received wrong BLE intro packet and discarded.");
  }
}

@end
