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

#import "internal/platform/implementation/apple/Mediums/BLE/Sockets/Tests/Peripheral/GNSFakePeripheralManager.h"

@implementation GNSFakePeripheralManager

- (instancetype)initWithDelegate:(id<CBPeripheralManagerDelegate>)delegate
                           queue:(dispatch_queue_t)queue
                         options:(NSDictionary<NSString *, id> *)options {
  self = [super init];
  if (self) {
    _delegate = delegate;
    _services = [NSMutableArray array];
    _state = CBPeripheralManagerStateUnknown;
  }
  return self;
}

- (void)addService:(CBMutableService *)service {
  [_services addObject:service];
  _addServiceCount++;
  if ([_delegate respondsToSelector:@selector(peripheralManager:didAddService:error:)]) {
    [_delegate peripheralManager:(CBPeripheralManager *)self didAddService:service error:nil];
  }
}

- (void)removeService:(CBMutableService *)service {
  [_services removeObject:service];
}

- (void)removeAllServices {
  [_services removeAllObjects];
  _removeAllServicesCount++;
}

- (void)startAdvertising:(NSDictionary<NSString *, id> *)advertisementData {
  _advertisementData = advertisementData;
  _advertising = YES;
  _startAdvertisingCount++;
  if ([_delegate respondsToSelector:@selector(peripheralManagerDidStartAdvertising:error:)]) {
    [_delegate peripheralManagerDidStartAdvertising:(CBPeripheralManager *)self error:nil];
  }
}

- (void)stopAdvertising {
  _advertisementData = nil;
  _advertising = NO;
  _stopAdvertisingCount++;
}

- (void)respondToRequest:(CBATTRequest *)request withResult:(CBATTError)result {
  _lastRespondRequest = request;
  _lastRespondResult = result;
}

- (BOOL)updateValue:(NSData *)value
       forCharacteristic:(CBMutableCharacteristic *)characteristic
    onSubscribedCentrals:(NSArray<CBCentral *> *)centrals {
  return YES;
}

@end
