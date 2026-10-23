// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not us/e this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#import "internal/platform/implementation/apple/Mediums/BLE/Tests/GNCMFakeConnection.h"

#import "internal/platform/implementation/apple/Mediums/BLE/GNCBLEGATTCharacteristic.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCMConnection.h"
#import "internal/platform/implementation/apple/Mediums/BLE/GNCPeripheral.h"

@implementation GNCMFakeConnection
@synthesize isConnected = _isConnected;
@synthesize maxDataLength = _maxDataLength;
@synthesize characteristic = _characteristic;
@synthesize peripheral = _peripheral;
@synthesize connectionHandlers = _connectionHandlers;

- (instancetype)init {
  self = [super init];
  if (self) {
    _isConnected = YES;
    _maxDataLength = 20;
  }
  return self;
}

- (void)disconnect {
  _isConnected = NO;
}

- (void)sendData:(NSData *)payload
    progressHandler:(GNCMProgressHandler)progressHandler
         completion:(GNCMPayloadResultHandler)completion {
  if (completion) {
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
      completion(GNCMPayloadResultSuccess);
    });
  }
}

- (void)simulateDataReceived:(NSData *)data {
  if (_connectionHandlers && _connectionHandlers.payloadHandler) {
    _connectionHandlers.payloadHandler(data);
  }
}

- (void)simulateDisconnect {
  _isConnected = NO;
  if (_connectionHandlers && _connectionHandlers.disconnectedHandler) {
    _connectionHandlers.disconnectedHandler();
  }
}

@end
