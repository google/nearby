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

#import "internal/platform/implementation/ios/Mediums/Ble/GNCMBleConnection.h"

#import "internal/platform/implementation/ios/Mediums/Ble/GNCMBleUtils.h"
#import "internal/platform/implementation/ios/Mediums/Ble/Sockets/Source/Shared/GNSSocket.h"
#import "internal/platform/implementation/ios/Mediums/GNCLeaks.h"
#import "internal/platform/implementation/ios/Mediums/GNCMConnection.h"
#import "GoogleToolboxForMac/GTMLogger.h"

NS_ASSUME_NONNULL_BEGIN

@interface GNCMBleConnection () <GNSSocketDelegate>
@property(nonatomic) dispatch_queue_t selfQueue;
@property(nonatomic) GNSSocket *socket;
@property(nonatomic) NSData *serviceIdHash;
@property(nonatomic) dispatch_queue_t callbackQueue;
@end

@implementation GNCMBleConnection

+ (instancetype)connectionWithSocket:(GNSSocket *)socket
                           serviceId:(NSString *)serviceId
                       callbackQueue:(dispatch_queue_t)callbackQueue {
  GNCMBleConnection *connection = [[GNCMBleConnection alloc] init];
  connection.socket = socket;
  socket.delegate = connection;
  connection.serviceIdHash = GNCMServiceIdHash(serviceId);
  connection.callbackQueue = callbackQueue;
  connection.selfQueue = dispatch_queue_create("GNCMBleConnectionQueue", DISPATCH_QUEUE_SERIAL);
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
    [_socket sendData:data
        progressHandler:^(float progress) {
          // Convert normalized progress value to number of bytes.
          dispatch_async(_callbackQueue, ^{
            progressHandler((size_t)(progress * data.length));
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
  dispatch_async(_selfQueue, ^{
    if (_connectionHandlers.payloadHandler) {
      dispatch_async(_callbackQueue, ^{
        _connectionHandlers.payloadHandler(data);
      });
    }
  });
}

@end

NS_ASSUME_NONNULL_END
