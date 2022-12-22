// Copyright 2022 Google LLC
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

#import "internal/platform/implementation/apple/Mediums/Ble/GNCMBleConnection.h"

#import "internal/platform/implementation/apple/Mediums/Ble/GNCMBleUtils.h"
#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Shared/GNSSocket.h"
#import "internal/platform/implementation/apple/Mediums/GNCLeaks.h"
#import "internal/platform/implementation/apple/Mediums/GNCMConnection.h"
#import "GoogleToolboxForMac/GTMLogger.h"

NS_ASSUME_NONNULL_BEGIN

@interface GNCMBleConnection () <GNSSocketDelegate>
@property(nonatomic) dispatch_queue_t selfQueue;
@property(nonatomic) GNSSocket *socket;
@property(nonatomic) NSData *serviceIDHash;
@property(nonatomic) dispatch_queue_t callbackQueue;
@property(nonatomic) BOOL expectedIntroPacket;
@property(nonatomic) BOOL receivedIntroPacket;
@end

@implementation GNCMBleConnection

+ (instancetype)connectionWithSocket:(GNSSocket *)socket
                           serviceID:(nullable NSString *)serviceID
                 expectedIntroPacket:(BOOL)expectedIntroPacket
                       callbackQueue:(dispatch_queue_t)callbackQueue {
  GNCMBleConnection *connection = [[GNCMBleConnection alloc] init];
  connection.socket = socket;
  socket.delegate = connection;
  connection.serviceIDHash = serviceID ? GNCMServiceIDHash(serviceID) : nil;
  connection.callbackQueue = callbackQueue;
  connection.selfQueue = dispatch_queue_create("GNCMBleConnectionQueue", DISPATCH_QUEUE_SERIAL);
  connection.expectedIntroPacket = expectedIntroPacket;
  return connection;
}

- (void)dealloc {
  [_socket disconnect];
  GNCVerifyDealloc(_socket, 2);  // assert if not deallocated
}

#pragma mark GNCMConnection
- (void)sendData:(NSData *)data
    progressHandler:(GNCMProgressHandler)progressHandler
         completion:(GNCMPayloadResultHandler)completion {
  dispatch_async(_selfQueue, ^{
    NSMutableData *packet;
    if (data.length == 0) {
      // Get the Control introduction packet if data length is 0.
      NSData *introData = GNCMGenerateBLEFramesIntroductionPacket(_serviceIDHash);
      packet = [NSMutableData dataWithData:introData];
    } else {
      // Prefix the service ID hash.
      packet = [NSMutableData dataWithData:_serviceIDHash];
      [packet appendData:data];
    }

    [_socket sendData:packet
        progressHandler:^(float progress) {
          // Convert normalized progress value to number of bytes.
          dispatch_async(_callbackQueue, ^{
            progressHandler((size_t)(progress * packet.length));
          });
        }
        completion:^(NSError *error) {
          dispatch_async(_callbackQueue, ^{
            completion(error ? GNCMPayloadResultFailure : GNCMPayloadResultSuccess);
          });
        }];
  });
}

#pragma mark GNSSocketDelegate

- (void)socketDidConnect:(GNSSocket *)socket {
  GTMLoggerError(@"Unexpected -socketDidConnect: call; should've already happened");
}

- (void)socket:(GNSSocket *)socket didDisconnectWithError:(NSError *)error {
  dispatch_async(_selfQueue, ^{
    if (_connectionHandlers.disconnectedHandler) {
      dispatch_async(_callbackQueue, ^{
        _connectionHandlers.disconnectedHandler();
      });
    }
  });
}

- (void)socket:(GNSSocket *)socket didReceiveData:(NSData *)data {
  // Extract the service ID prefix from each data packet.
  NSMutableData *packet;
  NSUInteger prefixLength = _serviceIDHash.length;
  if (_expectedIntroPacket && !_receivedIntroPacket) {
    // Check if the first packet is intro packet.
    if (!_serviceIDHash) {
      // If _serviceIdHash is nil, then we need to parse the first incoming packet if it conforms to
      // introducion packet and extract the serviceIdHash for coming packets.
      NSData *serviceIDHash = GNCMParseBLEFramesIntroductionPacket(data);
      if (serviceIDHash) {
        _serviceIDHash = serviceIDHash;
        _receivedIntroPacket = YES;
      } else {
        GTMLoggerInfo(@"[NEARBY] Input stream: Received wrong intro packet and discarded");
      }
    } else {
      NSData *introData = GNCMGenerateBLEFramesIntroductionPacket(_serviceIDHash);
      if ([data isEqual:introData]) {
        _receivedIntroPacket = YES;
      } else {
        GTMLoggerInfo(@"[NEARBY] Input stream: Received wrong intro packet and discarded");
      }
    }
    return;
  }

  if (![[data subdataWithRange:NSMakeRange(0, prefixLength)] isEqual:_serviceIDHash]) {
    GTMLoggerInfo(@"[NEARBY] Input stream: Received wrong data packet and discarded");
    return;
  }
  packet = [NSMutableData
      dataWithData:[data subdataWithRange:NSMakeRange(prefixLength, data.length - prefixLength)]];

  dispatch_async(_selfQueue, ^{
    if (_connectionHandlers.payloadHandler) {
      dispatch_async(_callbackQueue, ^{
        _connectionHandlers.payloadHandler(packet);
      });
    }
  });
}

@end

NS_ASSUME_NONNULL_END
