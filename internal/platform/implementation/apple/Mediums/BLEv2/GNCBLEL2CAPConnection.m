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

  // Validate the L2CAP packet.
  // TODO: b/399815436 - Refactor the validation logic to connections layer.
 
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
  if (data.length < kL2CAPPacketLength) {
    GTMLoggerError(@"[NEARBY] Packet length mismatch. Expected: > %d, Actual: %lu",
                   kL2CAPPacketLength, data.length);
    return nil;
  }
  int realDataLength = CFSwapInt32BigToHost(
      *(int *)([[data subdataWithRange:NSMakeRange(0, kL2CAPPacketLength)] bytes]));
  if (realDataLength != (data.length - kL2CAPPacketLength)) {
    GTMLoggerError(@"[NEARBY] Data length mismatch. Expected: %d, Actual: %lu",
                   realDataLength, data.length - kL2CAPPacketLength);
    return nil;
  }
  return [data subdataWithRange:NSMakeRange(kL2CAPPacketLength, data.length - kL2CAPPacketLength)];
}

@end
