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

enum { kL2CAPPacketLength = 4 };
static const CGFloat kRequestDataConnectionDelayInSeconds = 0.0;
static const UInt8 kRequestDataConnectionTimeoutInSeconds = 5;

static char *const kGNCBLEL2CAPConnectionQueueLabel = "com.google.nearby.GNCBLEL2CAPConnection";

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
@property(nonatomic) BOOL handledIncomingConnectionL2CAPPacket;
@property(nonatomic) BOOL handledOutgoingConnectionL2CAPPacket;
@property(nonatomic) BOOL handledReceivedL2CAPRequestDataConnectionPacket;
@property(nonatomic) BOOL handledReceivedL2CAPResponseDataConnectionReadyPacket;
@property(nonatomic) BOOL handledReceivedBLEIntroPacket;
@property(nonatomic) NSUInteger expectedDataLength;
@property(nonatomic) NSMutableData *undeliveredData;
@property(nonatomic) BOOL verboseLoggingEnabled;
@property(nonatomic) NSCondition *requestDataConnectionCondition;
@end

@implementation GNCBLEL2CAPConnection

+ (instancetype)connectionWithStream:(GNCBLEL2CAPStream *)stream
                           serviceID:(NSString *)serviceID
                  incomingConnection:(BOOL)incomingConnection
                       callbackQueue:(dispatch_queue_t)callbackQueue {
  GNCBLEL2CAPConnection *connection = [[GNCBLEL2CAPConnection alloc] init];
  [connection setStream:stream];
  stream.delegate = connection;
  [connection setServiceIDHash:GNCMServiceIDHash(serviceID)];
  [connection setCallbackQueue:callbackQueue];
  [connection
      setSelfQueue:dispatch_queue_create(kGNCBLEL2CAPConnectionQueueLabel,
                                         dispatch_queue_attr_make_with_qos_class(
                                             DISPATCH_QUEUE_SERIAL, QOS_CLASS_USER_INITIATED, -1))];
  [connection setIncomingConnection:incomingConnection];
  [connection setUndeliveredData:[NSMutableData data]];
  [connection setExpectedDataLength:0];
  [connection setRequestDataConnectionCondition:[[NSCondition alloc] init]];
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
    if (_verboseLoggingEnabled) {
      GTMLoggerDebug(@"[NEARBY] GNCBLEL2CAPConnection data to be sent: %@", [packet description]);
    }
    [_stream sendData:packet
        completionBlock:^(BOOL result) {
          dispatch_async(_callbackQueue, ^{
            completion(result);
          });
        }];
  });
}

- (void)requestDataConnectionWithCompletion:(void (^)(BOOL))completion {
  GTMLoggerInfo(@"[NEARBY] Sending l2cap packet request data connection");
  // TODO b/399815436 - A bug is causing channel has written to the socket but the remote does not
  // receive it. Add a delay to make sure the data is written to the socket. Remove the delay once
  // the bug is fixed.
  dispatch_time_t requestTime =
      dispatch_time(DISPATCH_TIME_NOW, kRequestDataConnectionDelayInSeconds * NSEC_PER_SEC);
  dispatch_after(requestTime, _selfQueue, ^(void) {
    [_requestDataConnectionCondition lock];
    NSData *requestDataConnectionPacket =
        GNCMGenerateBLEL2CAPPacket(GNCMBLEL2CAPCommandRequestDataConnection, nil);
    [_stream sendData:PrefixLengthData(requestDataConnectionPacket)
        completionBlock:^(BOOL result){

        }];
    dispatch_async(_callbackQueue, ^{
      NSDate *requestDataConnectionTimeout =
          [NSDate dateWithTimeIntervalSinceNow:kRequestDataConnectionTimeoutInSeconds];
      BOOL result = [_requestDataConnectionCondition waitUntilDate:requestDataConnectionTimeout];
      completion(result);
      [_requestDataConnectionCondition unlock];
    });
  });
}

#pragma mark GNCBLEL2CAPStreamDelegate

- (void)stream:(GNCBLEL2CAPStream *)stream didReceiveData:(NSData *)data {
  if (_verboseLoggingEnabled) {
    GTMLoggerDebug(@"[NEARBY] BLEL2CAPConnection didReceiveData, data: %@, length: %lu",
                   [data debugDescription], data.length);
  }
  if (!data.length) {
    return;
  }

  __weak __typeof__(self) weakSelf = self;
  dispatch_async(_selfQueue, ^{
    __typeof__(self) strongSelf = weakSelf;
    if (!strongSelf) {
      return;
    }
    [strongSelf->_undeliveredData appendData:data];

    while (strongSelf->_undeliveredData.length) {
      NSUInteger bytesProcessed = [self processReceivedPacket];
      if (bytesProcessed == 0) {
        // Don't have a full packet yet, wait for more data from device.
        break;
      }
      // Clear processed data.
      strongSelf->_expectedDataLength = 0;
      [strongSelf->_undeliveredData replaceBytesInRange:NSMakeRange(0, bytesProcessed)
                                              withBytes:nil
                                                 length:0];
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

- (NSUInteger)processReceivedPacket {
  dispatch_assert_queue(_selfQueue);

  NSUInteger bytesProcessed = 0;
  NSData *realData = [self extractRealDataFromData:_undeliveredData];
  if (!realData) {
    return bytesProcessed;
  }
  bytesProcessed = realData.length + kL2CAPPacketLength;

  // Validate the L2CAP packet.
  // TODO: b/399815436 - Refactor the validation logic to connections layer.
  if (_incomingConnection) {
    if (!_handledIncomingConnectionL2CAPPacket) {
      [self handleIncomingConnectionL2CAPPacketFromData:realData];
      return bytesProcessed;
    }
    if (!_handledReceivedBLEIntroPacket) {
      [self handleBLEIntroPacketFromData:realData];
      return bytesProcessed;
    }
  } else {
    if (!_handledOutgoingConnectionL2CAPPacket) {
      [self handleOutgoingConnectionL2CAPPacketFromData:realData];
      return bytesProcessed;
    }
  }

  // Extract the service ID prefix from each data packet and validate it.
  NSUInteger prefixLength = _serviceIDHash.length;
  if (![[realData subdataWithRange:NSMakeRange(0, prefixLength)] isEqual:_serviceIDHash]) {
    GTMLoggerError(@"[NEARBY]: Received wrong data packet and discarded");
    return bytesProcessed;
  }

  if (_connectionHandlers.payloadHandler) {
    dispatch_async(_callbackQueue, ^{
      _connectionHandlers.payloadHandler([NSData
          dataWithData:[realData subdataWithRange:NSMakeRange(prefixLength,
                                                              realData.length - prefixLength)]]);
    });
  }
  return bytesProcessed;
}

/// Checks whether |data| at least has length |_expectedDataLength| and returns the real data;
/// Returns nil in case it has not enough data.
- (NSData *_Nullable)extractRealDataFromData:(NSData *)data {
  // Store the expected data length if it is not set yet.
  if (!_expectedDataLength) {
    if (data.length < kL2CAPPacketLength) {
      GTMLoggerError(@"[NEARBY] Data length mismatch. Expected: > %d, Actual: %lu",
                     kL2CAPPacketLength, data.length);
      return nil;
    }
    _expectedDataLength = CFSwapInt32BigToHost(
        *(int *)([[data subdataWithRange:NSMakeRange(0, kL2CAPPacketLength)] bytes]));
  }
  NSUInteger realDataLength = data.length - kL2CAPPacketLength;
  if (realDataLength < _expectedDataLength) {
    GTMLoggerError(@"[NEARBY] Insufficient data. Expected: %lu, Actual: %lu", _expectedDataLength,
                   realDataLength);
    return nil;
  } else if (realDataLength == _expectedDataLength) {
    return [data subdataWithRange:NSMakeRange(kL2CAPPacketLength, realDataLength)];
  } else {
    // This is a case where the data length is larger than the expected length.
    // The first |_expectedDataLength| bytes are copied and used as the actual data. The
    // remaining bytes will be stored in _undeliveredData for future use.
    GTMLoggerInfo(@"[NEARBY] Data length mismatch. Expected: %lu, Actual: %lu", _expectedDataLength,
                  realDataLength);
    return [data subdataWithRange:NSMakeRange(kL2CAPPacketLength, _expectedDataLength)];
  }
}

- (void)handleIncomingConnectionL2CAPPacketFromData:(NSData *)data {
  GNCMBLEL2CAPPacket *l2capPacket = GNCMParseBLEL2CAPPacket(data);
  if (!l2capPacket) {
    return;
  }
  switch (l2capPacket.command) {
    case GNCMBLEL2CAPCommandRequestDataConnection: {
      dispatch_async(_selfQueue, ^{
        [_stream sendData:PrefixLengthData(GNCMGenerateBLEL2CAPPacket(
                              GNCMBLEL2CAPCommandResponseDataConnectionReady, nil))
            completionBlock:^(BOOL result){
            }];
      });
      _handledReceivedL2CAPRequestDataConnectionPacket = YES;
    } break;
    case GNCMBLEL2CAPCommandRequestAdvertisement:
    case GNCMBLEL2CAPCommandRequestAdvertisementFinish:
    case GNCMBLEL2CAPCommandResponseAdvertisement:
    case GNCMBLEL2CAPCommandResponseServiceIdNotFound:
    case GNCMBLEL2CAPCommandResponseDataConnectionReady:
    case GNCMBLEL2CAPCommandResponseDataConnectionFailure:
    default:
      break;  // fall through
  }
  _handledIncomingConnectionL2CAPPacket = _handledReceivedL2CAPRequestDataConnectionPacket;
}

- (void)handleOutgoingConnectionL2CAPPacketFromData:(NSData *)data {
  GNCMBLEL2CAPPacket *l2capPacket = GNCMParseBLEL2CAPPacket(data);
  if (!l2capPacket) {
    return;
  }
  switch (l2capPacket.command) {
    case GNCMBLEL2CAPCommandResponseDataConnectionReady: {
      [_requestDataConnectionCondition lock];
      dispatch_async(_selfQueue, ^{
        [_stream sendData:PrefixLengthData(GNCMGenerateBLEFramesIntroductionPacket(_serviceIDHash))
            completionBlock:^(BOOL result) {
              [_requestDataConnectionCondition broadcast];
              [_requestDataConnectionCondition unlock];
            }];
      });
      _handledReceivedL2CAPResponseDataConnectionReadyPacket = YES;
    } break;
    case GNCMBLEL2CAPCommandRequestAdvertisement:
    case GNCMBLEL2CAPCommandRequestDataConnection:
    case GNCMBLEL2CAPCommandRequestAdvertisementFinish:
    case GNCMBLEL2CAPCommandResponseAdvertisement:
    case GNCMBLEL2CAPCommandResponseServiceIdNotFound:
    case GNCMBLEL2CAPCommandResponseDataConnectionFailure:
    default:
      break;  // fall through
  }
  _handledOutgoingConnectionL2CAPPacket = _handledReceivedL2CAPResponseDataConnectionReadyPacket;
}

- (void)handleBLEIntroPacketFromData:(NSData *)data {
  NSData *serviceIDHash = GNCMParseBLEFramesIntroductionPacket(data);
  if ([serviceIDHash isEqual:_serviceIDHash]) {
    _handledReceivedBLEIntroPacket = YES;
    GTMLoggerInfo(@"[NEARBY]: Received BLE intro packet and handled.");
  } else {
    GTMLoggerInfo(@"[NEARBY]: Received wrong BLE intro packet and discarded.");
  }
}

@end
